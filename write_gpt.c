#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>
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

 //FAT32 Volume Boot Record (VBR) structure
 typedef struct {
    uint8_t BS_jmpBoot[3];
    uint8_t BS_OEMName[8];
    uint16_t BPB_BytsPerSec;
    uint8_t BPB_SecPerClus;
    uint16_t BPB_RsvdSecCnt;
    uint8_t BPB_NumFATs;
    uint16_t BPB_RootEntCnt;
    uint16_t BPB_TotSec16;
    uint8_t BPB_Media;
    uint16_t BPB_FATSz16;
    uint16_t BPB_SecPerTrk;
    uint16_t BPB_NumHeads;
    uint32_t BPB_HiddSec;
    uint32_t BPB_TotSec32;
    uint32_t BPB_FATSz32;
    uint16_t BPB_ExtFlags;
    uint16_t BPB_FSVer;
    uint32_t BPB_RootClus;
    uint32_t BPB_FSInfo;
    uint32_t BPB_BkBootSec;
    uint8_t BPB_Reserved[12];
    uint8_t BS_DrvNum;
    uint8_t BS_Reserved1;
    uint8_t BS_BootSig;
    uint32_t BS_VolID;
    uint8_t BS_VolLab[11];
    uint8_t BS_FilSysType[8];

    // Not in fatgen103.doc tables
    uint8_t boot_code[510-90];
    uint16_t bootsect_sig; //boot sector signature 0xAA55
 } __attribute__((packed)) Fat32_Vbr;

 typedef struct {
    uint32_t FSI_LeadSig;
    uint8_t FSI_Reserved1[480];
    uint32_t FSI_StructSig;
    uint32_t FSI_Free_Count;
    uint32_t FSI_Nxt_Free;
    uint8_t FSI_Reserved2[12];
    uint32_t FSI_TrailSig;
 }__attribute__((packed)) FSInfo;

 typedef struct {
    uint8_t  DIR_Name[11];
    uint8_t  DIR_Attr;
    uint8_t  DIR_NTRes;
    uint8_t  DIR_CrtTimeTenth;
    uint16_t DIR_CrtTime;
    uint16_t DIR_CrtDate;
    uint16_t DIR_LstAccDate;
    uint16_t DIR_FstClusHI;
    uint16_t DIR_WrtTime;
    uint16_t DIR_WrtDate;
    uint16_t DIR_FstClusLO;
    uint32_t DIR_FileSize;
 }__attribute__((packed)) FAT32_Dir_Entry_Short;

 // FAT32 Directory Entry Attributes
typedef enum {
    ATTR_READ_ONLY = 0x01,
    ATTR_HIDDEN    = 0x02,
    ATTR_SYSTEM    = 0x04,
    ATTR_VOLUME_ID = 0x08,
    ATTR_DIRECTORY = 0x10,
    ATTR_ARCHIVE   = 0x20,
    ATTR_LONG_NAME = ATTR_READ_ONLY | ATTR_HIDDEN |
                     ATTR_SYSTEM    | ATTR_VOLUME_ID,
} FAT32_Dir_Attr;


// FAT32 File "types"
typedef enum {
    TYPE_DIR,   // Directory
    TYPE_FILE,  // Regular file
} File_Type;

// Common Virtual Hard Disk Footer, for a "fixed" vhd
// All fields are in network byte order (Big Endian),
//   since I'm lazy or otherwise a bad programmer,
//   we'll use byte arrays here
typedef struct {
    uint8_t cookie[8];
    uint8_t features[4];
    uint8_t version[4];
    uint64_t data_offset;
    uint8_t timestamp[4];
    uint8_t creator_app[4];
    uint8_t creator_ver[4];
    uint8_t creator_OS[4];
    uint8_t original_size[8];
    uint8_t current_size[8];
    uint8_t disk_geometry[4];
    uint8_t disk_type[4];
    uint8_t checksum[4];
    GUID unique_id;
    uint8_t saved_state;
    uint8_t reserved[427];
} __attribute__ ((packed)) Vhd;

// Internal Options object for commandline args
typedef struct {
    char *image_name;
    uint32_t lba_size;
    uint32_t esp_size;
    uint32_t data_size;
    char **esp_file_paths;
    uint32_t num_esp_file_paths;
    FILE **esp_files;
    char **data_files;
    uint32_t num_data_files;
    bool vhd;
    bool help;
    bool error;
} Options;

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
uint64_t gpt_table_lbas = 0; //LBA of GPT table      
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

void get_fat_dir_time_date(uint16_t *in_time, uint16_t *in_date)
{
    time_t curr_time = time(NULL);
    struct tm tm= *localtime(&curr_time);


    *in_date = ((tm.tm_year - 80) << 9) | ((tm.tm_mon + 1) << 5) | tm.tm_mday;
    *in_time = (tm.tm_hour << 11) | (tm.tm_min << 5) | (tm.tm_sec / 2);

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
        .first_usable_lba = 1+ 1 + gpt_table_lbas, //MBR + GPT header + primary GPT table
        .last_usable_lba = image_size_lbas - 1 - 1 - gpt_table_lbas, //TODO: Calculate this properly
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
            .name = {'E', 'F', 'I', ' ', 'S', 'Y', 'S', 'T', 'E', 'M'},
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

        // Fill out primary header CRC32
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
        secondary_gpt.partition_table_lba = image_size_lbas - 1 - gpt_table_lbas; //Secondary GPT table is before secondary GPT header
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


bool write_esp(FILE *image)
{
    //Reserved sector region -------------------------------
    //Write ESP system partition with FAT32 filesystem
    const uint8_t reserved_sectors = 32; //FAT32 spec says this should be at least 32 sectors, but can be more
    Fat32_Vbr vbr = {
        .BS_jmpBoot = { 0xEB, 0x58, 0x90 }, //Jump instruction to boot code
        .BS_OEMName = { 'M', 'S', 'W', 'I', 'N', '4', '.', '1' },
        .BPB_BytsPerSec = lba_size, //Bytes per sector (Can be changed 512/1024/2048/4096)
        .BPB_SecPerClus = 1, //TODO: Calculate this properly
        .BPB_RsvdSecCnt = reserved_sectors, //TODO: Calculate this properly
        .BPB_NumFATs = 2, //Number of FATs
        .BPB_RootEntCnt = 0, //FAT32 doesn't use this
        .BPB_TotSec16 = 0, //FAT32 doesn't use this
        .BPB_Media = 0xF8, //Fixed disk (could be 0xF0 for removable media like flash drives)
        .BPB_FATSz16 = 0, //FAT32 doesn't use this
        .BPB_SecPerTrk = 0, //TODO: Calculate this properly
        .BPB_NumHeads = 0, //TODO: Calculate this properly
        .BPB_HiddSec = esp_lba, //num of sectors before this partition (MBR + GPT header + GPT table)
        .BPB_TotSec32 = esp_size_lbas, //Total number of sectors
        .BPB_FATSz32 = (align_lba - reserved_sectors) / 2, //TODO: Calculate this properly
        .BPB_ExtFlags = 0, //Mirrored FATs, active FAT is FAT0
        .BPB_FSVer = 0, //FAT32 version 0.0
        .BPB_RootClus = 2, //Root directory starts at cluster 2
        .BPB_FSInfo = 1, //Sector 0 = this VBR
        .BPB_BkBootSec = 6,
        .BPB_Reserved = { 0 },
        .BS_DrvNum = 0x80, //1st drive
        .BS_Reserved1 = 0,
        .BS_BootSig = 0x29,
        .BS_VolID = 0,
        .BS_VolLab = { "NO NAME    "}, // Currently no volume label TODO: Add a volume label
        .BS_FilSysType = {"FAT32   "}, 

        .boot_code = { 0 },
        .bootsect_sig = 0xAA55,


    };

    // Fill out file system info sector
    FSInfo fsinfo ={
        .FSI_LeadSig = 0x41615252,
        .FSI_Reserved1 = { 0 },
        .FSI_StructSig = 0x61417272,
        .FSI_Free_Count = 0xFFFFFFF, //Test
        .FSI_Nxt_Free = 0xFFFFFFFF, //Test
        .FSI_Reserved2 = { 0 },
        .FSI_TrailSig = 0xAA550000,
    };

    //Write VBR and FSInfo
    fseek(image, esp_lba * lba_size, SEEK_SET);
    if (fwrite(&vbr, 1, sizeof(vbr), image) != sizeof(vbr))
    {
        fprintf(stdout, "Error: VBR didn't write to img\n");
        return false;
    }
    write_full_lba_size(image);


    if (fwrite(&fsinfo, 1, sizeof(fsinfo), image) != sizeof(fsinfo))
    {
        fprintf(stdout, "Error: FSInfo didn't write to img\n");
        return false;
    }
    write_full_lba_size(image);


    //go to backup boot sector location
    fseek(image, (esp_lba + vbr.BPB_BkBootSec) * lba_size, SEEK_SET);

    if (fwrite(&vbr, 1, sizeof(vbr), image) != sizeof(vbr))
    {
        fprintf(stdout, "Error: VBR didn't write to img\n");
        return false;
    }
    write_full_lba_size(image);

    if (fwrite(&fsinfo, 1, sizeof(fsinfo), image) != sizeof(fsinfo))
    {
        fprintf(stdout, "Error: FSInfo didn't write to img\n");
        return false;
    }
    write_full_lba_size(image);


    //FAT region -------------------------------
    //TODO: Write FATs
    fseek(image, (esp_lba + vbr.BPB_RsvdSecCnt) * lba_size, SEEK_SET);
    const uint32_t fat_lba = esp_lba + vbr.BPB_RsvdSecCnt;
    for (uint8_t i = 0; i < vbr.BPB_NumFATs; i++)
    {
        fseek(image, (fat_lba + (i * vbr.BPB_FATSz32)) * lba_size, SEEK_SET);

        uint32_t cluster = 0;

        //Cluster 1, FAT indicator, lowest 8 bits are media byte
        cluster = 0xFFFFFF00 | vbr.BPB_Media;
        fwrite(&cluster, sizeof(cluster), 1, image);

        //EOC marker (Cluster 2)
        cluster = 0xFFFFFFFF;
        fwrite(&cluster, sizeof(cluster), 1, image);

        //Cluster 3 Root dir cluster start
        cluster = 0xFFFFFFFF;
        fwrite(&cluster, sizeof(cluster), 1, image);

        //Cluster 4 '/EFI' dir cluster
        cluster = 0xFFFFFFFF;
        fwrite(&cluster, sizeof(cluster), 1, image);

        //Cluster 5+ Other files/dirs
        cluster = 6; //Points to next cluster with file data
        cluster = 0xFFFFFFFF; //indicates no more file data after this cluster
    }

    //Data region -------------------------------
    //Write File data
    const uint32_t data_lba = fat_lba + (vbr.BPB_NumFATs * vbr.BPB_FATSz32);
    fseek(image, data_lba * lba_size, SEEK_SET);

    //TODO Root '/' Dir
    FAT32_Dir_Entry_Short dir_ent = {
        .DIR_Name = {"EFI        "},
        .DIR_Attr = ATTR_DIRECTORY,
        .DIR_NTRes = 0,
        .DIR_CrtTimeTenth = 0,
        .DIR_CrtTime = 0,
        .DIR_CrtDate = 0,
        .DIR_LstAccDate = 0,
        .DIR_FstClusHI = 0, //Hopefully should always be 0
        .DIR_WrtTime = 0,
        .DIR_WrtDate = 0,
        .DIR_FstClusLO = 3,
        .DIR_FileSize = 0,

    };

    uint16_t time = 0;
    uint16_t date = 0;
    get_fat_dir_time_date(&time, &date);

    dir_ent.DIR_CrtTime = time;
    dir_ent.DIR_CrtDate = date;
    dir_ent.DIR_WrtTime = time;
    dir_ent.DIR_WrtDate = date;

    fwrite(&dir_ent, sizeof(dir_ent), 1, image);

    //TODO /EFI Dir
    fseek(image, (data_lba + 1) * lba_size, SEEK_SET);
    memcpy(dir_ent.DIR_Name, ".          ", 11);
    fwrite(&dir_ent, sizeof(dir_ent), 1, image);

    memcpy(dir_ent.DIR_Name, "..         ", 11);
    dir_ent.DIR_FstClusLO = 0;
    fwrite(&dir_ent, sizeof(dir_ent), 1, image);

    memcpy(dir_ent.DIR_Name, "BOOT       ", 11);
    dir_ent.DIR_FstClusLO = 4;
    fwrite(&dir_ent, sizeof(dir_ent), 1, image);

    //TODO /EFI/BOOT Dir
    fseek(image, (data_lba + 2) * lba_size, SEEK_SET);

    memcpy(dir_ent.DIR_Name, ".          ", 11);
    fwrite(&dir_ent, sizeof(dir_ent), 1, image);

    memcpy(dir_ent.DIR_Name, "..         ", 11);
    dir_ent.DIR_FstClusLO = 3;
    fwrite(&dir_ent, sizeof(dir_ent), 1, image);

    


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
    const uint64_t padding = (ALIGNMENT*2 + (lba_size* gpt_table_lbas*2) + 1 + 2);

    //Set sizes
    image_size = esp_size + data_size + padding; // Add some extra padding for GPT
    image_size_lbas = bytes_to_lbas(image_size);
    align_lba = ALIGNMENT / lba_size;
    esp_lba = align_lba; //ESP starts after alignment
    esp_size_lbas = bytes_to_lbas(esp_size);
    data_size_lbas = bytes_to_lbas(data_size);
    data_lba = next_aligned_lba(esp_lba + esp_size_lbas); //Data partition starts after ESP
    gpt_table_lbas = GPT_TABLE_SIZE/ lba_size; //amount of LBAs used by GPT table

    

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

    //TODO: Write ESP system partiton with FAT32 filesystem

    if (!write_esp(image))
    {
        fprintf(stderr, "Error: could not write ESP for file %s\n", image_name);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;

}