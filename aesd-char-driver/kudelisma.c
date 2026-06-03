#include <stdio.h>
#include <stdbool.h>
// #include <linux/types.h> this is the user version: #include <stdbool.h>
#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif
// #include <linux/string.h> this is the user version: #include <string.h>
#include <fcntl.h>
#include <stdint.h>
#include <linux/kernel.h>
#include <linux/module.h>

typedef char unsigned u8;
typedef short unsigned u16;
typedef int unsigned u32;
typedef long long unsigned u64;
typedef char s8;
typedef short s16;
typedef int s32;
typedef long long s64;
// typedef int bool; for kernel in bool
//#define true 1
// #define false 0 for kernel in bool
#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))
#define min(a,b) (((a) < (b)) ? (a) : (b))
#define max(a,b) (((a) > (b)) ? (a) : (b))
#define OUT_SZ 256



enum grp {ax, bx, cx, dx, sp, bp, si, di, ip, ss, ds, es, fl}; // general purpose registers
enum flag_bits {o=4,d=5,i=6,t=7,s=8,z=9,a=11,p=13,c=15};

// transfered to maps
// enum flag_bits_masker {o_masker=0b100000000000,d_masker=0b10000000000,i_masker=0b1000000000,t_masker=0b100000000,s_masker=0b10000000,
//     z_masker=0b1000000,a_masker=0b10000,p_masker=0b100,c_masker=0b1};

u16 general_purpose_registers[16]; // 12 is the flag

enum MnemonicId {
    MN_UNDEF = 0,
    MN_MOV,
    MN_ADD,
    MN_SUB,
    MN_CMP,
    MN_JNE,
    MN_COUNT
};

enum data_transfer_type_num {
    rm_tf_fr = 0,
    i_t_rm = 1,
    i_t_r = 2,
    m_t_a = 3,
    a_t_m = 4
};

static const char *mnemonic_names[MN_COUNT] = {
     "UNDEF", "mov", "add", "sub", "cmp", "jne"
};
static u8 mnemonic_index[256];

void init_table(void){
    int i;
    for (i = 0; i < 256; ++i){
        mnemonic_index[i] = MN_UNDEF;
    }
    mnemonic_index[0b10001000] = 1;
    mnemonic_index[0b10110000] = 1;
    mnemonic_index[0b11000110] = 1;
    mnemonic_index[0b10100000] = 1;
    mnemonic_index[0b10100010] = 1;
    mnemonic_index[0] = 2;
    mnemonic_index[0b10000000] = 2;
    mnemonic_index[0b00000100] = 2;
    mnemonic_index[0b00101000] = 3;
    mnemonic_index[0b00101100] = 3;
    mnemonic_index[0b00101100] = 3;
    mnemonic_index[0b00111000] = 4;
    mnemonic_index[0b00111100] = 4;
    mnemonic_index[0b01110101] = 5;
    //mnemonic_index[0b00111100] = 4;
}

//static const char *mnemonic_names[MN_COUNT] = {
//     "UNDEF", "mov", "add", "sub", "cmp", "jne"
//};
//static u8 reg_index[32];
//void init_register(void){
//
//}


typedef struct{
    u8 key;
    const char *value;
} umap;

typedef struct{
    u8 key;
    const char *value;
} umap_r;

typedef struct{
    char *mnemonic;
    u8 data_transfer_type;
    bool destination;
    bool wide;
    bool displacement;
    bool data_avail;
    bool has_add;
} lmi_form; // left_machine_instruction_form

typedef struct{
    u16 data;
    u16 disp_data;
    char *s_reg;
    char *s_rm;
    bool bracket;
    bool sw;
} lmi_form_right; // left_machine_instruction_form


umap nem[] = {
    {0b100010, "mov"}, // Register/memory to/from register
    {0b1100011, "mov"},// immediate to register/memory
    {0b1011, "mov"}, // immediate to register
    {0b1010000, "mov"}, // memory to accumulator
    {0b1010001, "mov"}, // accumulator to memory
    {0b10001110, "mov"}, // Register/memory to segment register
    {0b10001100, "mov"},// Segmenet register to register/memory
    // ----------------------------------------------------------------//
    {0b0, "add"},
    {0b100000, "add"},
    {0b0000010, "add"},
    // ----------------------------------------------------------------//
    {0b001010, "sub"},
    // {0b100000, "sub"},
    {0b0010110, "sub"},
    // ----------------------------------------------------------------//
    {0b001110, "cmp"},
    {0b0011110, "cmp"},
    // ----------------------------------------------------------------//
    {0b01110101, "jne"}
};

//umap reg_w[] = {
//    {0b000, "ax"},
//    {0b001, "cx"},
//    {0b010, "dx"},
//    {0b011, "bx"},
//    // _________________ space for readability
//    {0b100, "sp"},
//    {0b101, "bp"},
//    {0b110, "si"},
//    {0b111, "di"},
//};

static const char *reg_w[9] = {
    "ax", "cx", "dx", "bx", "sp", "bp", "si", "di", ""
};

static const char *reg_nw[9] = {
    "al", "cl", "dl", "bl", "ah", "ch", "dh", "bh", ""
};


static const char *rm_00[9] = {
    "bx + si", "bx + di", "bp + si", "bp + di", "si", "di", "bp", "bx", ""
};

//static const char *rm_01[8] = {
//    "al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"
//};
//
//static const char *rm_10[8] = {
//    "al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"
//};

//umap reg_nw[] = {
//    {0b000, "al"},
//    {0b001, "cl"},
//    {0b010, "dl"},
//    {0b011, "bl"},
//    // _________________ space for readability
//    {0b100, "ah"},
//    {0b101, "ch"},
//    {0b110, "dh"},
//    {0b111, "bh"},
//};

//umap rm[] = {
//    {0b000, "bx + si"},
//    {0b001, "bx + di"},
//    {0b010, "bp + si"},
//    {0b011, "bp + di"},
//    // _________________ space for readability
//    {0b100, "si"},
//    {0b101, "di"},
//    {0b110, "bp"}, // if mod == 00 it is direct address
//    {0b111, "bx"},
//};

static const char *rm[8] = {
    "bx + si", /* 0 */
    "bx + di", /* 1 */
    "bp + si", /* 2 */
    "bp + di", /* 3 */
    "si",      /* 4 */
    "di",      /* 5 */
    "bp",      /* 6 -> special when mod == 0 */
    "bx"       /* 7 */
};



void printBinary(unsigned char num) {
    printf("0b");
    for (int i = 7; i >= 0; i--) {
        u8 mask = 1 << i;
        printf("%c", (num & mask) ? '1' : '0');
    }
    printf(", ");
    //printf("\n");
}

void copy_string(char *dest, const char *src, size_t max_len) {
    int i = 0;
    while (src[i] != '\0' && i < (int)max_len - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0'; // null terminate
}


static void right_encoder(lmi_form myForm, u8 champiArray[], u8 counter[], char* out){

    // R/M field encoding. since we will call string writer/
    lmi_form_right myForm_right;

    u16 index = (u16)counter[0];
    u8 input2;
    u8 input_mod;
    u8 input_reg;
    u8 input_rm;
    char s_reg[16];
    char s_rm[16];


    bool direct_address = false;
    myForm_right.s_reg = s_reg;
    myForm_right.s_rm = s_rm;
    myForm_right.sw = false;



    if(myForm.data_transfer_type == rm_tf_fr){
        input2 = champiArray[index + 1];
        input_mod = (0b11000000 & input2) >> 6; // not yet implemented
        input_reg = (0b00111000 & input2) >> 3;
        input_rm = (0b00000111 & input2);
        if(input_mod == 0){
            // for direct address
            if(input_rm == 6){
                myForm.displacement = false;
                myForm.data_avail = true;
                input_mod = 7;
                direct_address = true;
            }else{
                myForm.displacement = false;
                myForm_right.disp_data = 0;
            }
        }
    }
    else if(myForm.data_transfer_type == i_t_rm){
        input2 = champiArray[index+1];
        input_mod = (0b11000000 & input2) >> 6;
        input_reg = 8;
        input_rm = champiArray[index + 1] & 0b00000111;
        if(input_mod == 0 || input_mod == 3){
            myForm.displacement = false;
            myForm_right.disp_data = 0;
        }
        if(myForm.has_add == true){

            if(myForm.destination == 0 && myForm.wide == 1){
                myForm_right.sw = true;
            }

        }

    }
    else if(myForm.data_transfer_type == i_t_r){
        input2 = champiArray[index];
        input_mod = 6;
        input_reg = champiArray[index] & 0b00000111;
        myForm_right.bracket = false;
    }
    else if(myForm.data_transfer_type == m_t_a){
        input_mod = 6;
        input_reg = 0;
        if(myForm.has_add){
            myForm_right.bracket = false;
        }else{
            myForm_right.bracket = true;
        }
        myForm.data_transfer_type = i_t_r;
    }
    else if(myForm.data_transfer_type == a_t_m){
        input_mod = 6;
        input_reg = 0;
        if(myForm.has_add){
            myForm_right.bracket = false;
        }else{
            myForm_right.bracket = true;
        }
        myForm.data_transfer_type = i_t_r;
    }
    else{
        return;
    }


    switch(input_mod) {
        case 0b11: {
            if(myForm.wide){
                copy_string(myForm_right.s_reg, reg_w[input_reg] ,sizeof(myForm_right.s_reg));
                copy_string(myForm_right.s_rm, reg_w[input_rm] ,sizeof(myForm_right.s_rm));
            }else{
                copy_string(myForm_right.s_reg, reg_nw[input_reg] ,sizeof(myForm_right.s_reg));
                copy_string(myForm_right.s_rm, reg_nw[input_rm] ,sizeof(myForm_right.s_rm));
            }
            myForm_right.disp_data = 0;
            myForm_right.bracket = false;

            if(myForm.data_avail == true){
                if(myForm.destination == false && myForm.wide == true){
                    myForm_right.data = (champiArray[index + 3] << 8) ^ champiArray[index + 2];
                    counter[0]+=2;
                }else{
                    myForm_right.data = champiArray[index + 2];
                    counter[0]+=1;
                }
            }

            writer_overall(myForm, myForm_right, out, s_reg, s_rm);


        } break;
        case 0b10: {

            if( myForm.wide ){
                copy_string(myForm_right.s_reg, reg_w[input_reg] ,sizeof(myForm_right.s_reg));
                copy_string(myForm_right.s_rm, rm_00[input_rm] ,sizeof(myForm_right.s_rm));
            }else {
                copy_string(myForm_right.s_reg, reg_nw[input_reg] ,sizeof(myForm_right.s_reg));
                copy_string(myForm_right.s_rm, rm_00[input_rm] ,sizeof(myForm_right.s_rm));
            }

            u8 adder = index;

            if(myForm.data_avail == true){
                myForm_right.disp_data = (champiArray[index + 3] << 8) ^ champiArray[index + 2];
                if( myForm.wide ){
                    myForm_right.data = (champiArray[index + 5] << 8) ^ champiArray[index + 4];
                    counter[0]+=2; // for data
                }else{
                    myForm_right.data = champiArray[index + 4];
                    counter[0]+=1; // for data
                }
            }else{
                myForm_right.disp_data = (champiArray[index + 3] << 8) ^ champiArray[index + 2];
            }
            counter[0]+=2; // this is for displacement

            // myForm_right.data = (champiArray[index + 3] << 8) ^ champiArray[index + 2];
            myForm_right.bracket = true;
            writer_overall(myForm, myForm_right, out);

        } break;
        case 0b01: {
            if( myForm.wide ){
                copy_string(myForm_right.s_reg, reg_w[input_reg] ,sizeof(myForm_right.s_reg));
                copy_string(myForm_right.s_rm, rm_00[input_rm] ,sizeof(myForm_right.s_rm));
                // printf("YAYAKAPIN: %s\n", s_reg);
            }else {
                copy_string(myForm_right.s_reg, reg_nw[input_reg] ,sizeof(myForm_right.s_reg));
                copy_string(myForm_right.s_rm, rm_00[input_rm] ,sizeof(myForm_right.s_rm));
            }

            u8 adder = index + 1;

            if(myForm.displacement == true){
                adder+=1;
                myForm_right.disp_data = champiArray[adder];
                // myForm_right.disp_data = (champiArray[index + 3] << 8) ^ champiArray[index + 2];
                counter[0]+=1;
            }



            if(myForm.data_avail == true){
                if(myForm.wide){
                    adder+=2;
                    myForm_right.data = (champiArray[adder] << 8) ^ champiArray[adder-1];
                }else{
                    adder += 1;
                    myForm_right.data = champiArray[adder];
                }
            }
            // myForm_right.data = champiArray[index + 2];
            myForm_right.bracket = true;
            writer_overall(myForm, myForm_right, out);
            // champiArray[index + 2] is u8 and the function called is u16 may cause problem later
        } break;
        case 0: {
            u8 adder = index + 1;
            if( myForm.wide ){
                if(myForm.has_add && input_rm == 0b110){
                    // char stringified_numeric_disp_data[20];
                    u16 foo = (champiArray[adder + 2] << 8) ^ champiArray[adder + 1];
                    adder+=2;
                    copy_string(myForm_right.s_reg, reg_w[input_reg] ,sizeof(myForm_right.s_reg));
                    snprintf(myForm_right.s_rm, sizeof(myForm_right.s_rm), "%d", foo);
                    counter[0]+=2;

                }else{
                    copy_string(myForm_right.s_reg, reg_w[input_reg] ,sizeof(myForm_right.s_reg));
                    copy_string(myForm_right.s_rm, rm_00[input_rm] ,sizeof(myForm_right.s_rm));
                }
                // printf("YAYAKAPIN: %s\n", s_reg);
            }else {
                if(myForm.has_add && input_rm == 0b110){
                    u16 foo = champiArray[adder + 1];
                    adder+=1;
                    copy_string(myForm_right.s_reg, reg_nw[input_reg] ,sizeof(myForm_right.s_reg));
                    snprintf(myForm_right.s_rm, sizeof(myForm_right.s_rm), "%d", foo);
                    counter[0]+=1;
                }else{
                    copy_string(myForm_right.s_reg, reg_nw[input_reg] ,sizeof(myForm_right.s_reg));
                    copy_string(myForm_right.s_rm, rm_00[input_rm] ,sizeof(myForm_right.s_rm));
                }

            }

            if(myForm.data_avail == true){
                if(myForm.has_add){
                    if(myForm_right.sw){
                        myForm_right.data = (champiArray[adder + 2] << 8) ^ champiArray[adder + 1];
                        counter[0]+=2; // for data
                        adder+=2;
                    }else{
                        myForm_right.data = champiArray[adder + 1];
                        counter[0]+=1; // for data
                        adder+=1;
                    }
                }else{
                    if( myForm.wide ){
                        myForm_right.data = (champiArray[adder + 2] << 8) ^ champiArray[adder + 1];
                        counter[0]+=2; // for data
                        adder+=2;
                    }else{
                        myForm_right.data = champiArray[adder + 1];
                        counter[0]+=1; // for data
                        adder+=1;
                    }
                }

            }
            myForm_right.bracket = true;
            writer_overall(myForm, myForm_right, out);
        } break;
        case 5: {
            if( myForm.wide ){
                copy_string(myForm_right.s_rm, rm_00[input_rm] ,sizeof(myForm_right.s_rm));
                myForm_right.data = (champiArray[index + 3] << 8) ^ champiArray[index + 2];
                counter[0]+=2;
            }else {
                copy_string(myForm_right.s_rm, rm_00[input_rm] ,sizeof(myForm_right.s_rm));
                myForm_right.data = champiArray[index + 2];
                counter[0]+=1;
            }
            myForm_right.bracket = true;
            writer_overall(myForm, myForm_right, out);
        } break;
        case 6: {
            if( myForm.wide ){
                copy_string(myForm_right.s_reg, reg_w[input_reg] ,sizeof(myForm_right.s_reg));
                myForm_right.data = (champiArray[index + 2] << 8) ^ champiArray[index + 1];
                counter[0]+=1;
            }else {
                copy_string(myForm_right.s_reg, reg_nw[input_reg] ,sizeof(myForm_right.s_reg));
                myForm_right.data = champiArray[index + 1];
            }
            // myForm_right.bracket = false;
            writer_overall(myForm, myForm_right, out);
        } break;
        case 7: {
            if( myForm.wide ){
                copy_string(myForm_right.s_reg, reg_w[input_reg] ,sizeof(myForm_right.s_reg));
                // myForm_right.data = (champiArray[index + 3] << 8) ^ champiArray[index + 2];
            }else {
                copy_string(myForm_right.s_reg, reg_nw[input_reg] ,sizeof(myForm_right.s_reg));
                // myForm_right.data = champiArray[index + 2];
            }
            myForm_right.data = (champiArray[index + 3] << 8) ^ champiArray[index + 2];
            myForm_right.bracket = true;
            myForm.data_transfer_type = i_t_r;
            writer_overall(myForm, myForm_right, out);
            counter[0]+=2;
        } break;
        default: {
            return;
        } break;

    };

}

void writer_overall(lmi_form myForm, lmi_form_right myForm_right, char *out){
    // bits have 4 choices, 3 for ins to ins, 2 for 16 bits, 1 for 8 bits, 0 for no integer
    // if(myForm_right.data == 0){ myForm_right.bracket = false; }


    char stringified_numeric_data[20];
    char stringified_numeric_disp_data[20];

    if(myForm.displacement){
        snprintf(stringified_numeric_disp_data, sizeof(stringified_numeric_disp_data), "%d", myForm_right.disp_data);
    }else{
        snprintf(stringified_numeric_disp_data, sizeof(stringified_numeric_disp_data), "%d", 0);
    }

    if(myForm.data_avail){
        snprintf(stringified_numeric_data, sizeof(stringified_numeric_data), "%d", myForm_right.data);
    }else{
        snprintf(stringified_numeric_data, sizeof(stringified_numeric_data), "%d", 0);
    }

    // printf("GAGOBI: %s\n", stringified_numeric_data);

    out[0] = '\0';
    size_t rem;

    // mnemonic
    rem = OUT_SZ - strlen(out) - 1;
    strncat(out, myForm.mnemonic, rem);

    // space
    rem = OUT_SZ - strlen(out) - 1;
    strncat(out, " ", rem);

    if(myForm.data_transfer_type == i_t_r){

        if(myForm.destination == 1){
            rem = OUT_SZ  - strlen(out) - 1;
            strncat(out, myForm_right.s_reg, rem);
            rem = OUT_SZ  - strlen(out) - 1;
            if( myForm_right.bracket ){
                strncat(out, ", [", rem);
            }else{
                strncat(out, ", ", rem);
            }

            rem = OUT_SZ  - strlen(out) - 1;
            strncat(out, stringified_numeric_data, rem);
            rem = OUT_SZ  - strlen(out) - 1;
            if( myForm_right.bracket ){
                strncat(out, "]", rem);
            }
        }
        else{
            rem = OUT_SZ  - strlen(out) - 1;
            if( myForm_right.bracket ){
                strncat(out, "[", rem);
            }
            rem = OUT_SZ  - strlen(out) - 1;
            strncat(out, stringified_numeric_data, rem);
            if( myForm_right.bracket ){
                strncat(out, "], ", rem);
            }else{
                strncat(out, ", ", rem);
            }
            rem = OUT_SZ  - strlen(out) - 1;
            strncat(out, myForm_right.s_reg, rem);
        }


        return;
    }
    else if(myForm.data_transfer_type == i_t_rm){
        if(myForm_right.bracket == true){
            rem = OUT_SZ  - strlen(out) - 1;
            strncat(out, "[", rem);
        }

        rem = OUT_SZ  - strlen(out) - 1;
        strncat(out, myForm_right.s_rm, rem);

        if(myForm_right.disp_data != 0){
            rem = OUT_SZ  - strlen(out) - 1;
            strncat(out, " + ", rem);

            rem = OUT_SZ  - strlen(out) - 1;
            strncat(out, stringified_numeric_disp_data, rem);
        }

        if(myForm_right.bracket == true){
            rem = OUT_SZ  - strlen(out) - 1;
            strncat(out, "]", rem);
        }

        rem = OUT_SZ  - strlen(out) - 1;
        strncat(out, ", ", rem);

        rem = OUT_SZ  - strlen(out) - 1;

        if(myForm_right.bracket == true){
            if(myForm.wide){
                strncat(out, "word ", rem);
            }else{
                strncat(out, "byte ", rem);
            }
        }

        rem = OUT_SZ  - strlen(out) - 1;
        strncat(out, stringified_numeric_data, rem);
        return;
    }

    if(myForm.destination){

        // first operand
        rem = OUT_SZ  - strlen(out) - 1;
        strncat(out, myForm_right.s_reg, rem);

        // comma+space
        rem = OUT_SZ  - strlen(out) - 1;
        if( myForm_right.bracket ){
            strncat(out, ", [", rem);
        }else{

        }


        //  second operand
        rem = OUT_SZ  - strlen(out) - 1;
        strncat(out, myForm_right.s_rm, rem);

        if(myForm_right.bracket){
            if(myForm_right.disp_data != 0){
                rem = OUT_SZ  - strlen(out) - 1;
                strncat(out, " + ", rem);
                rem = OUT_SZ  - strlen(out) - 1;
                strncat(out, stringified_numeric_disp_data, rem);
            }

            rem = OUT_SZ  - strlen(out) - 1;
            strncat(out, "]", rem);
        }

        return;

    }else{
        if( myForm_right.bracket ){
            strncat(out, "[", rem);
        }
        // first operand
        rem = OUT_SZ  - strlen(out) - 1;
        strncat(out, myForm_right.s_rm, rem);

        if( myForm_right.bracket ){
            if(myForm_right.disp_data != 0){
                rem = OUT_SZ  - strlen(out) - 1;
                strncat(out, " + ", rem);
                rem = OUT_SZ  - strlen(out) - 1;
                strncat(out, stringified_numeric_disp_data, rem);
            }
            strncat(out, "]", rem);
        }

        // comma+space
        rem = OUT_SZ  - strlen(out) - 1;
        strncat(out, ", ", rem);

        //  second operand
        rem = OUT_SZ  - strlen(out) - 1;
        strncat(out, myForm_right.s_reg, rem);


        return;
    }
}



void left_and_right_encoding(u8 champiArray[], u8 counter[], char* out){
    u16 index = (u16)counter[0];
    u8 input = 0 + champiArray[index];

    for (int i = 0; i < 8; ++i) {
        // MN_UNDEF
        if(mnemonic_index[input] != MN_UNDEF) { // if shit is not equal to zero it exists. otherwise tapyas pa
            break;
        }
        // printf("mnemonic_index: %d\n", input);

        input = input >> (i + 1);
        input = input << (i + 1);
    }

    char *cur_nem = mnemonic_names[mnemonic_index[input]];
    lmi_form myForm;
    myForm.has_add = false;

    switch(input){
        case 0:
        case 0b00101000:
        case 0b00111000:
        case 0b10001000:  {
            // left encoding
            bool destination = (0b00000010 & champiArray[index]) << 1;
            bool wide = (0b00000001 & champiArray[index]);

            myForm.destination = (0b00000010 & champiArray[index]) << 1;
            myForm.wide = (0b00000001 & champiArray[index]);
            myForm.mnemonic = mnemonic_names[mnemonic_index[input]];
            myForm.data_transfer_type = rm_tf_fr;
            myForm.displacement = true;
            myForm.data_avail = false;

            char s_reg[256];
            char s_rm[256];


            right_encoder(myForm, champiArray, counter, out);

            return;
        } break;


        case 0b11000110: { // immediate to register memory
            myForm.wide = (champiArray[index] & 0b00000001);
            myForm.destination = 1;
            myForm.mnemonic = mnemonic_names[mnemonic_index[input]];
            myForm.data_transfer_type = i_t_rm;
            myForm.displacement = true;
            myForm.data_avail = true;

            right_encoder(myForm, champiArray, counter, out);
            return;
        } break;


        case 0b10110000: {
            bool wide = (champiArray[index] & 0b00001000) >> 3;
            myForm.destination = 1;
            myForm.wide = wide;
            myForm.mnemonic = mnemonic_names[mnemonic_index[input]];
            myForm.data_transfer_type = i_t_r;
            myForm.displacement = false;
            myForm.data_avail = true;

            right_encoder(myForm, champiArray, counter, out);
            return;

        } break;

        case 0b00000100:
        case 0b00101100:
        case 0b00111100:
        case 0b10100000: {
            // cmp's data if s:w=1 is the same in add and sub 01. the 01 is what should follow
            myForm.wide = (champiArray[index] & 0b00000001);
            myForm.destination = 1;
            myForm.mnemonic = mnemonic_names[mnemonic_index[input]];
            myForm.data_transfer_type = m_t_a;
            myForm.displacement = false;
            myForm.data_avail = true;
            if(input == 0b00000100 || 0b00101100){
                myForm.has_add = true;
            }

            right_encoder(myForm, champiArray, counter, out);
            return;

        } break;

        case 0b10100010: {
            myForm.wide = (champiArray[index] & 0b00000001);
            myForm.destination = 0;
            myForm.mnemonic = mnemonic_names[mnemonic_index[input]];
            myForm.data_transfer_type = a_t_m;
            myForm.displacement = false;
            myForm.data_avail = true;

            right_encoder(myForm, champiArray, counter, out);
            return;

        } break;

//        case 0: {
//            myForm.destination = (0b00000010 & champiArray[index]) << 1;
//            myForm.wide = (0b00000001 & champiArray[index]);
//        } break;

        case 0b10000000: {
            myForm.destination = (0b00000010 & champiArray[index]) << 1;
            myForm.wide = (0b00000001 & champiArray[index]);

            u8 input2 = champiArray[index + 1];
            u8 input_reg = (0b00111000 & input2) >> 3;

            if(input_reg == 0b101){
                myForm.mnemonic = mnemonic_names[mnemonic_index[0b00101000]]; // for sub
            }else if(input_reg == 0b111){
                myForm.mnemonic = mnemonic_names[mnemonic_index[0b00111000]]; // for cmp
            }else{
                myForm.mnemonic = mnemonic_names[mnemonic_index[input]];
            }
            myForm.data_transfer_type = i_t_rm;
            myForm.displacement = true;
            myForm.data_avail = true;
            myForm.has_add = true;

            char s_reg[256];
            char s_rm[256];

            right_encoder(myForm, champiArray, counter, out);

            return;

        } break;

        case 0b01110101: {
            // there is no out of bounds error yet
            // this area is about jumps
            char stringified_numeric_data[20];
            u8 data_counter = champiArray[index + 1];

            out[0] = '\0';
            size_t rem;

            rem = OUT_SZ - strlen(out) - 1;
            strncat(out, mnemonic_names[mnemonic_index[0b01110101]], rem);


            if(data_counter > 128){ // negative
                data_counter = 256-data_counter;
                data_counter-=2;
                snprintf(stringified_numeric_data, sizeof(stringified_numeric_data), "%d", data_counter);

                rem = OUT_SZ - strlen(out) - 1;
                strncat(out, " $", rem);
                rem = OUT_SZ - strlen(out) - 1;
                strncat(out, "-", rem);
                rem = OUT_SZ - strlen(out) - 1;
                strncat(out, stringified_numeric_data, rem);


                counter[0]-=data_counter;
                counter[0]-=2;
            }else{
                data_counter+=2;
                snprintf(stringified_numeric_data, sizeof(stringified_numeric_data), "%d", data_counter);

                rem = OUT_SZ - strlen(out) - 1;
                strncat(out, " $", rem);
                rem = OUT_SZ - strlen(out) - 1;
                strncat(out, "+", rem);
                rem = OUT_SZ - strlen(out) - 1;
                strncat(out, stringified_numeric_data, rem);


                counter[0]+=data_counter;
                counter[0]-=2;
                // bale data, tapos yung -2 pang puksa sa kopal+=2
            }


            // counter[0]+=data_counter;

        } break;


    };

}


// int main(int argc, char **argv)
int main()
{
    init_table();

    // u8 champiHex[] = {0b10001001, 0b11011001, 0b10001001, 0b11011001, 0b10001001, 0b11011001};
    // after finishing all it will be in argv

    s32 data_fd = open("/home/aa/Documents/computer_enhance_perfaware/listing_0041_add_sub_cmp_jnz", O_RDWR | O_APPEND, 0600);
    printf("%d\n", data_fd);
    if (data_fd < 0) {
        perror("open");
        return 1;
    }

    unsigned char buf[1];
    char result[OUT_SZ];
    ssize_t n;
    off_t data_fd_size = lseek(data_fd, 0, SEEK_END);
    lseek(data_fd, 0, SEEK_SET);
    u8 champiHex[data_fd_size];
    ssize_t bytes_read = read(data_fd, champiHex, data_fd_size);
    u8 champiHex1[] = {0x75, 0x02,0x75, 0xfc,0x75, 0xfa}; // 0xc7, 0x85, 0x85, 0x03, 0x5b, 0x01,
    for(int i = 0; i < ArrayCount(champiHex); i++){
        // printf("GAGO! %d\n", champiHex[i]);
        // printf("%d, ", champiHex[i]);
        // printf("GAGO! %d\n", champiHex[i]);
        // printBinary(champiHex[i]);
    }

    printf("File size: %ld bytes\n", data_fd_size);
    u8 kopal[] = {0, 0};
    // while( (kopal[0] < data_fd_size) && (n = read(data_fd, buf, 1)) == 1){
    while( kopal[0] < ArrayCount(champiHex1) ){ // data_fd_size or ArrayCount(champiHex1)
        left_and_right_encoding(champiHex1, kopal, result);

        // printBinary(champiHex[kopal[0]]);
        // printBinary(champiHex[kopal[0] + 1]);
        kopal[0]+=2; // minimum i think
        printf(" result: %s kopal: %d, kopal: %d\n", result, kopal[0], kopal[1]);
    }
    close(data_fd);



    printf("Hello world! %d\n", data_fd);
    return 0;
}






















