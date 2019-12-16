clang -g ul_thread.c -shared -fpic -o libul_thread.so
clang -g main.c -lul_thread -o main -L/home/rmb122/temp/ult
export LD_LIBRARY_PATH=/lib:/usr/lib:/home/rmb122/temp/ult
