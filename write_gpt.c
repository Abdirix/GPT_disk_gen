#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <uchar.h> //for UCS-2 (UTF-16) string support
//NOTE: This code is written for 512 byte sectors, and will not work for 4096 byte sectors without modification


// GUID/UUID structure
typedef struct {
    uint32_t time_lo;
    uint16_t time_mid;
    uint16_t time_hi_and_version; // Upper 4 bits are version #
    uint8_t clock_seq_hi_and_reserved; // Upper 2 bits are variant #
    uint8_t clock_seq_lo;
    uint8_t node[6];
} __attribute__((packed)) GUID;

// MBR Parition structure
typedef struct {
    uint8_t boot_indicator;
    uint8_t starting_chs[3];
    uint8_t os_type;
    uint8_t ending_chs[3];
    uint32_t starting_lba;
    uint32_t size_lba;
} __attribute__((packed)) Mbr_Partition;

//Master Boot Record structure
typedef struct {
    uint8_t boot_code[440];
    uint32_t mbr_signature;
    uint16_t unknown;
    Mbr_Partition partition[4];
    uint16_t boot_signature;
} __attribute__ ((packed)) Mbr;


typedef struct {
    uint8_t signature[8];
    uint32_t revision;
    uint32_t header_size;
    uint32_t header_crc32;
    uint32_t reserved_l;
    uint64_t my_lba;
    uint64_t alternate_lba;
    uint64_t first_usable_lba;
    uint64_t last_usable_lba;
    GUID disk_guid; //pseudorandomly generated Version 4 variant 2 GUID
    uint64_t partition_table_lba;
    uint32_t number_of_entries;
    uint32_t size_of_entry;
    uint32_t partition_table_crc32;

    uint8_t reserved_2[512-92];
} __attribute__((packed)) Gpt_Header;

 typedef struct {
    // TODO: Fill out partition entry structure
    GUID partition_type_guid;
    GUID unique_guid;
    uint64_t starting_lba;
    uint64_t ending_lba;
    uint64_t attributes;
    char16_t name[36]; //UCS-2 (UTF-16 limited to code points 0x0000-0xFFFF)
 } __attribute__((packed)) Gpt_Partition_Entry;

 //TODO: Find out why these const are written the way they are

    // EFI System Partition GUID
 const GUID ESP_GUID = { 0xC12A7328, 0xF81F, 0x11D2, 0xBA, 0x4B, 
    {0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B } };

    // (Microsoft) Basic Data Partition GUID
 const GUID BASIC_DATA_GUID = { 0xEBD0A0A2, 0xB9E5, 0x4433, 0x87, 0xC0, 
    { 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7 } };


 enum {
    GPT_TABLE_ENTRY_SIZE = 128, //size of one partition entry in bytes
    NUMBER_OF_GPT_TABLE_ENTRIES = 128, //number of partition entries in the GPT table
    GPT_TABLE_SIZE = 16384, //Minimum size per UEFI spec 2.10, 128 entries * 128 bytes per entry
    ALIGNMENT = 1048576 //1MB alignment for GPT tables

 };

char *image_name = "test.img";
uint64_t lba_size = 512;
uint64_t esp_size = 1024*1024*33;
uint64_t data_size = 1024*1024*1;
uint64_t image_size = 0;
uint64_t esp_size_lbas, data_size_lbas, image_size_lbas; //Sizes of LBAs
uint64_t align_lba = 0, esp_lba = 0, data_lba = 0; //Starting LBA values

uint64_t bytes_to_lbas(const uint64_t bytes)
{
    return (bytes / lba_size) + (bytes % lba_size > 0 ? 1 : 0);
}

void write_full_lba_size(FILE *image)
{ //Will fill the rest of image with 0s to make it a full LBA size
  //TODO: Need this to handle 4096 byte sectors as well, but for now just assume 512 byte sectors
    uint8_t zero_sector[512];
    for (uint8_t i =0; i < (lba_size - sizeof(zero_sector)) / sizeof(zero_sector); i++)
    {
        fwrite(zero_sector, sizeof(zero_sector), 1, image);
    }
}

uint64_t next_aligned_lba(uint64_t lba)
{
    return ((lba + align_lba - 1) / align_lba) * align_lba;
}

GUID new_guid(void)
{
    uint8_t rand_arr[16] = { 0 };
    for (uint8_t i = 0; i < sizeof(rand_arr); i++)
    {
        rand_arr[i] = rand() % 256; //Generates pesudorandom numbers between 0 and 255
    }

    GUID result = { //Fill out GUID
        //TODO: Make sure this is a Version 4 Variant 2 GUID
        .time_lo = (rand_arr[0] << 24) | (rand_arr[1] << 16) | (rand_arr[2] << 8) | rand_arr[3],
        .time_mid = (rand_arr[4] << 8) | rand_arr[5],
        .time_hi_and_version = ((rand_arr[6] & 0x0F) << 8) | rand_arr[7], //Upper 4 bits are version #
        .clock_seq_hi_and_reserved = (rand_arr[8] & 0x3F) | 0x80, //Upper 2 bits are variant #
        .clock_seq_lo = rand_arr[9],
        .node = { rand_arr[10], rand_arr[11], rand_arr[12], rand_arr[13], rand_arr[14], rand_arr[15] }
    };

    //Version bits (Version 4)
    result.time_hi_and_version &= ~(1 << 15); //0b_1_000 0000
    result.time_hi_and_version |= (1 << 14); //0b0_1_00 0000
    result.time_hi_and_version &= ~(1 << 13); //0b11_0_1 0000
    result.time_hi_and_version &= ~(1 << 12); //0b111_0_ 0000

    //Variant bits (Variant 2)
    result.clock_seq_hi_and_reserved |= (1 << 7); //0b_1_000 0000
    result.clock_seq_hi_and_reserved |= (1 << 6); //0b0_1_00 0000
    result.clock_seq_hi_and_reserved &= ~(1 << 5); //0b11_0_1 0000

    return result;
}

uint32_t crc32_table[256];

void create_crc32_table(void)
{
    uint32_t c;
    int32_t n, k;

    for (n = 0; n < 256; n++)
    {
        c = (uint32_t)n;
        for (k = 0; k < 8; k++)
        {
            if (c & 1)
                c = 0xEDB88320L ^ (c >> 1);
            else
                c = c >> 1;
        }
        crc32_table[n] = c;
    }
}

uint32_t calculate_crc32(void *buf, int32_t length)
{
    static bool made_crc_table = false;

    uint8_t *bufp = buf;
    uint32_t c = 0xFFFFFFFFL;
    int32_t n;

    if (!made_crc_table)
    {
        create_crc32_table();
        made_crc_table = true;
    }
    for (n = 0; n < length; n++)
    {
        c = crc32_table[(c ^ bufp[n]) & 0xFF] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFL;
}

bool write_mbr(FILE *image){
    //Write protective MBR for GPT
    //Don't want to change global image_size_lbas
    uint64_t mbr_image_lbas = image_size_lbas;
    if (mbr_image_lbas > 0xFFFFFFFF) mbr_image_lbas = 0x100000000;
    Mbr mbr = {
        .boot_code = { 0 },
        .mbr_signature = 0,
        .unknown = 0,
        .partition[0] = {
            .boot_indicator = 0,
            .starting_chs = {0x00, 0x02, 0x00}, //Doesn't matter for GPT
            .os_type = 0xEE, //This signals that this is a protective MBR for GPT
            .ending_chs = { 0xFF, 0xFF, 0xFF}, //Dosen't matter for GPT
            .starting_lba = 0x00000001, //first LBA after
            .size_lba = mbr_image_lbas - 1,
        },
        .boot_signature = 0xAA55,
    };

    //Write to file
    if (fwrite(&mbr, 1, sizeof(mbr), image) != sizeof(mbr)){
        return false;
    }
    write_full_lba_size(image);
    return true;
}

bool write_gpts(FILE *image)
{//Writes the primary GPT header
    Gpt_Header primary_gpt = {
        .signature = {'E', 'F', 'I', ' ', 'P', 'A', 'R', 'T'}, //Make sure no null terminator is added
        .revision = 0x00010000, //Version 1.0
        .header_size = 92,
        .header_crc32 = 0, //Will be calculated later
        .reserved_l = 0,
        .my_lba = 1, //Primary GPT header is always after MBR
        .alternate_lba = image_size_lbas - 1, //Second Primary GPT header
        .first_usable_lba = 1+ 1 + 32, //MBR + GPT header + primary GPT table
        .last_usable_lba = image_size_lbas - 1 - 1 - 32, //TODO: Calculate this properly
        .disk_guid = new_guid(), //TODO : Generate a random GUID
        .partition_table_lba = 2, //GPT table comes after GPT header
        .number_of_entries = 128, //TODO: Calculate this properly
        .size_of_entry = 128, //TODO: Calculate this properly
        .partition_table_crc32 = 0, //Will be calculated later
        .reserved_2 = {0},
        };


        // TODO: Fill out primary table with partitions
        //GPT header will refer to this same table, from two different places on disk
        Gpt_Partition_Entry gpt_table[NUMBER_OF_GPT_TABLE_ENTRIES] = 
        {
        //EFI System Partition
        {
            .partition_type_guid = ESP_GUID,
            .unique_guid = new_guid(),
            .starting_lba = esp_lba,
            .ending_lba = esp_lba + esp_size_lbas - 1,
            .attributes = 0,
            .name = u"EFI SYSTEM",
        },

        //Basic Data Partition
        {
            .partition_type_guid = BASIC_DATA_GUID,
            .unique_guid = new_guid(),
            .starting_lba = data_lba,
            .ending_lba = data_lba + data_size_lbas - 1,
            .attributes = 0,
            .name = u"BASIC DATA",
        },
    };

        // TODO: Fill out primary header CRC32
        primary_gpt.partition_table_crc32 = calculate_crc32(gpt_table, sizeof(gpt_table));
        primary_gpt.header_crc32 = calculate_crc32(&primary_gpt, primary_gpt.header_size);

        // TODO: Write primary GPT header and table to file
        if (fwrite(&primary_gpt, 1, sizeof(primary_gpt), image) != sizeof(primary_gpt))
        {
            return false;
        }
        write_full_lba_size(image); //fills rest of lba

        if (fwrite(&gpt_table, 1, sizeof(gpt_table), image) != sizeof(gpt_table))
        {
            return false;
        }

        // TODO: Fill out secondary GPT header
        Gpt_Header secondary_gpt = primary_gpt;
        secondary_gpt.partition_table_lba = image_size_lbas - 1 - 32;
        secondary_gpt.partition_table_crc32 = 0;
        secondary_gpt.header_crc32 = 0;
        secondary_gpt.my_lba = image_size_lbas - 1;
        secondary_gpt.alternate_lba = primary_gpt.my_lba;

        secondary_gpt.partition_table_crc32 = calculate_crc32(gpt_table, sizeof(gpt_table));
        secondary_gpt.header_crc32 = calculate_crc32(&secondary_gpt, secondary_gpt.header_size);

        // TODO: Go to positon of seconday table
        fseek(image, (secondary_gpt.partition_table_lba * lba_size), SEEK_SET);

        // TODO : Write secondary gpt table to file
        if (fwrite(&gpt_table, 1, sizeof(gpt_table), image) != sizeof(gpt_table))
        {
            return false;
        }

        // TODO : Write secondary gpt header to file
        if (fwrite(&secondary_gpt, 1, sizeof(secondary_gpt), image) != sizeof(secondary_gpt))
        {
            return false;
        }
        write_full_lba_size(image); //fills rest of lba

        return true;

    }



int main(void)
{
    FILE *image = fopen(image_name, "wb+");
    if (!image)
    {
        fprintf(stderr, "Error: file couldn't be opened %s\n", image_name);
        return EXIT_FAILURE;
    }

    //Allignemnt for GPT tables is 1MB, so make sure the image size is a multiple of 1MB
    const uint64_t padding = (ALIGNMENT*2 + (lba_size* 67));

    //Set sizes
    image_size = esp_size + data_size + padding; // Add some extra padding for GPT
    image_size_lbas = bytes_to_lbas(image_size);
    align_lba = ALIGNMENT / lba_size;
    esp_lba = align_lba; //ESP starts after alignment
    esp_size_lbas = bytes_to_lbas(esp_size);
    data_size_lbas = bytes_to_lbas(data_size);
    data_lba = next_aligned_lba(esp_lba + esp_size_lbas); //Data partition starts after ESP

    

    //Seed rand()
    srand(time(NULL));

    if (!write_mbr(image))
    {
        fprintf(stderr, "Error: could not write protective MBR for file %s\n", image_name);
        return EXIT_FAILURE;
    }

    //Write GPT headers & tables
    if (!write_gpts(image))
    {
        fprintf(stderr, "Error: could not write GPT headers and tables for file %s\n", image_name);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
    
}