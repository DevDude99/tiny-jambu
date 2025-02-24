gcc -o encrypt encrypt.c -Os -s -lcrypto
gcc -o decrypt decrypt.c -Os -s -lcrypto

echo "lorem ipsum" > plaintext.txt
./encrypt plaintext.txt dSXkEOoQ text.cipher
./decrypt text.cipher dSXkEOoQ output.txt
printf "output: `cat output.txt`\n"
