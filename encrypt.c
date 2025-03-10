/*
     TinyJAMBU-192: 192-bit key, 96-bit IV
     Optimized Implementation for 32-bit processor
     The state consists of four 32-bit registers
     state[3] || state[2] || state[1] || state[0]

     Implemented by Hongjun Wu
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "crypto_aead.h"

#define FrameBitsIV  0x10
#define FrameBitsAD  0x30
#define FrameBitsPC  0x50  //Framebits for plaintext/ciphertext
#define FrameBitsFinalization 0x70

#define NROUND1 128*5
#define NROUND2 128*9

#define MAX_KEY_LEN 24   // 192-bit key => 24 bytes
#define MAX_IV_LEN 12    // 96-bit IV => 12 bytes

/*optimized state update function*/
void state_update(unsigned int *state, const unsigned char *key, unsigned int number_of_steps)
{
	unsigned int i, j;
	unsigned int t1, t2, t3, t4;

	//in each iteration, we compute 128 rounds of the state update function.
	for (i = 0, j = 0; i < number_of_steps; i = i + 128)
	{
		t1 = (state[1] >> 15) | (state[2] << 17);  // 47 = 1*32+15
		t2 = (state[2] >> 6)  | (state[3] << 26);  // 47 + 23 = 70 = 2*32 + 6
		t3 = (state[2] >> 21) | (state[3] << 11);  // 47 + 23 + 15 = 85 = 2*32 + 21
		t4 = (state[2] >> 27) | (state[3] << 5);   // 47 + 23 + 15 + 6 = 91 = 2*32 + 27
		state[0] ^= t1 ^ (~(t2 & t3)) ^ t4 ^ ((unsigned int*)key)[j];
		j = j + 1;

		t1 = (state[2] >> 15) | (state[3] << 17);
		t2 = (state[3] >> 6)  | (state[0] << 26);
		t3 = (state[3] >> 21) | (state[0] << 11);
		t4 = (state[3] >> 27) | (state[0] << 5);
		state[1] ^= t1 ^ (~(t2 & t3)) ^ t4 ^ ((unsigned int*)key)[j];
		j = j + 1; if (j == 6) j = 0;  // it is to compute j = (j+1)%6

		t1 = (state[3] >> 15) | (state[0] << 17);
		t2 = (state[0] >> 6)  | (state[1] << 26);
		t3 = (state[0] >> 21) | (state[1] << 11);
		t4 = (state[0] >> 27) | (state[1] << 5);
		state[2] ^= t1 ^ (~(t2 & t3)) ^ t4 ^ ((unsigned int*)key)[j];
		j = j + 1;

		t1 = (state[0] >> 15) | (state[1] << 17);
		t2 = (state[1] >> 6)  | (state[2] << 26);
		t3 = (state[1] >> 21) | (state[2] << 11);
		t4 = (state[1] >> 27) | (state[2] << 5);
		state[3] ^= t1 ^ (~(t2 & t3)) ^ t4 ^ ((unsigned int*)key)[j];
		j = j + 1; if (j == 6) j = 0;  // it is to compute j = (j+1)%6
	}
}

// The initialization
/* The input to initialization is the 192-bit key; 96-bit IV;*/
void initialization(const unsigned char *key, const unsigned char *iv, unsigned int *state)
{
	int i;

	//initialize the state as 0
	for (i = 0; i < 4; i++) state[i] = 0;

	//update the state with the key
	state_update(state, key, NROUND2);

	//introduce IV into the state
	for (i = 0; i < 3; i++)
	{
		state[1] ^= FrameBitsIV;
		state_update(state, key, NROUND1);
		state[3] ^= ((unsigned int*)iv)[i];
	}
}

//process the associated data
void process_ad(const unsigned char *k, const unsigned char *ad, unsigned long long adlen, unsigned int *state)
{
	unsigned long long i;
	unsigned int j;

	for (i = 0; i < (adlen >> 2); i++)
	{
		state[1] ^= FrameBitsAD;
		state_update(state, k, NROUND1);
		state[3] ^= ((unsigned int*)ad)[i];
	}

	// if adlen is not a multiple of 4, we process the remaining bytes
	if ((adlen & 3) > 0)
	{
		state[1] ^= FrameBitsAD;
		state_update(state, k, NROUND1);
		for (j = 0; j < (adlen & 3); j++)  ((unsigned char*)state)[12 + j] ^= ad[(i << 2) + j];
		state[1] ^= adlen & 3;
	}
}

//encrypt a message
int crypto_aead_encrypt(
	unsigned char *c, unsigned long long *clen,
	const unsigned char *m, unsigned long long mlen,
	const unsigned char *ad, unsigned long long adlen,
	const unsigned char *nsec,
	const unsigned char *npub,
	const unsigned char *k
)
{
	unsigned long long i;
	unsigned int j;
	unsigned char mac[8];
	unsigned int state[4];

	//initialization stage
	initialization(k, npub, state);

	//process the associated data
	process_ad(k, ad, adlen, state);

	//process the plaintext
	for (i = 0; i < (mlen >> 2); i++)
	{
		state[1] ^= FrameBitsPC;
		state_update(state, k, NROUND2);
		state[3] ^= ((unsigned int*)m)[i];
		((unsigned int*)c)[i] = state[2] ^ ((unsigned int*)m)[i];
	}
	// if mlen is not a multiple of 4, we process the remaining bytes
	if ((mlen & 3) > 0)
	{
		state[1] ^= FrameBitsPC;
		state_update(state, k, NROUND2);
		for (j = 0; j < (mlen & 3); j++)
		{
			((unsigned char*)state)[12 + j] ^= m[(i << 2) + j];
			c[(i << 2) + j] = ((unsigned char*)state)[8 + j] ^ m[(i << 2) + j];
		}
		state[1] ^= mlen & 3;
	}

	//finalization stage, we assume that the tag length is 8 bytes
	state[1] ^= FrameBitsFinalization;
	state_update(state, k, NROUND2);
	((unsigned int*)mac)[0] = state[2];

	state[1] ^= FrameBitsFinalization;
	state_update(state, k, NROUND1);
	((unsigned int*)mac)[1] = state[2];

	*clen = mlen + 8;
	for (j = 0; j < 8; j++) c[mlen+j] = mac[j];

	return 0;
}
// Function to read the contents of the file into a buffer
unsigned char* read_file(const char* filename, unsigned long long* len) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open file");
        exit(1);
    }

    fseek(file, 0, SEEK_END);
    *len = ftell(file);
    fseek(file, 0, SEEK_SET);

    unsigned char* buffer = (unsigned char*)malloc(*len);
    if (!buffer) {
        perror("Memory allocation error");
        exit(1);
    }

    fread(buffer, 1, *len, file);
    fclose(file);
    return buffer;
}

// Function to write the encrypted contents to a file
void write_file(const char* filename, unsigned char* data, unsigned long long len) {
    FILE* file = fopen(filename, "wb");
    if (!file) {
        perror("Failed to open output file");
        exit(1);
    }

    fwrite(data, 1, len, file);
    fclose(file);
}

// Function to pad the key to 24 bytes
int pad_key(const char* key_str, unsigned char* key) {
    int key_len = strlen(key_str);

    if (key_len > MAX_KEY_LEN) {
        fprintf(stderr, "Error: Passphrase is too long. It must be at most 24 characters.\n");
        return -1;
    }

    for (int i = 0; i < key_len; i++) {
        key[i] = (unsigned char)key_str[i];
    }

    for (int i = key_len; i < MAX_KEY_LEN; i++) {
        key[i] = 0x00; // Zero-padding
    }

    return 0;
}

// Main function to handle encryption
int main(int argc, char* argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <input_file> <key> <output_file>\n", argv[0]);
        return 1;
    }

    const char* input_file = argv[1];
    const char* key_str = argv[2];
    const char* output_file = argv[3];

    unsigned char key[MAX_KEY_LEN];

    if (pad_key(key_str, key) != 0) {
        return 1;
    }
	unsigned char iv[MAX_IV_LEN] = { 0x00 };  // 96-bit IV, all zero for simplicity

    unsigned long long mlen;
    unsigned char* m = read_file(input_file, &mlen);

    unsigned char* c = (unsigned char*)malloc(mlen + 8);  // 8 bytes for the MAC
    if (!c) {
        perror("Memory allocation error");
        return 1;
    }

    unsigned long long clen;
    int encrypt_status = crypto_aead_encrypt(c, &clen, m, mlen, NULL, 0, NULL, iv, key);

    if (encrypt_status != 0) {
        fprintf(stderr, "Encryption failed.\n");
        return 1;
    }

    write_file(output_file, c, clen);

    free(m);
    free(c);

    printf("Encryption complete. Encrypted text saved to %s.\n", output_file);
    return 0;
}
