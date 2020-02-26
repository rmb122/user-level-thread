clang -fno-stack-protector -g ul_thread.c -shared -fpic -o libul_thread.so
clang -fno-stack-protector -g main.c -lul_thread -o main -L/home/rmb122/temp/ult
export LD_LIBRARY_PATH=/lib:/usr/lib:/home/rmb122/temp/ult
