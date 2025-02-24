# tiny-jambu
Tiny Jambu simple implementation in C

### Usage

Compile the encryptor and decryptor:
```
gcc -o encrypt encrypt.c -Os -s -lcrypto
gcc -o decrypt decrypt.c -Os -s -lcrypto
```

Encrypt file:

```
./encrypt plaintext.txt PASSWORD text.cipher
```

Decrypt file:

```
./decrypt text.cipher PASSWORD output.txt
```
