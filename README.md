## For windows use:
gcc -D_GNU_SOURCE -Wall -O2 -o camaraLinux camaraLinux.c -lX11


## For windows use:
g++ camara.cpp -o camara.exe -DUNICODE -D_UNICODE -lmfplat -lmf -lmfreadwrite -lmfuuid -lole32 -lgdi32 -luser32 -luuid   