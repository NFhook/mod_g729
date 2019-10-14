IPPARCH=em64t
LARCH=$(IPPARCH)
CC=cc 
FSBASE=/home/krivushinme/soft/freeswitch/fssvn
IPPBASE=/opt/intel/ipp/6.0.1.071/$(IPPARCH)
IPPCORE=m7

CC_OPT=-fPIC -O3 -DIPPCORE_STATIC_INIT -fomit-frame-pointer -march=nocona -fno-exceptions

INCLUDE=-I$(IPPBASE)/include -include "$(IPPBASE)/tools/staticlib/ipp_$(IPPCORE).h" -I$(FSBASE)/src/include -I$(FSBASE)/libs/libteletone/src

#INCLUDE=-I/opt/intel/ipp/6.0.1.071/ia32/include -include "/opt/intel/ipp/6.0.1.071/ia32/tools/staticlib/ipp_px.h" -I../../../include -I../../../../libs/libteletone/src

LDFLAGS =-Wl,-static -L"$(IPPBASE)/lib" -avoid-version -module -lippscmerged$(LARCH) -lippsrmerged$(LARCH) -lippsmerged$(LARCH) -lippcore$(LARCH) -liomp5


all : decg729.o encg729.o owng729.o vadg729.o aux_tbls.o libg729.o mod_g729.o
	# -shared -Xlinker -x -lfreeswitch -pthread 
	$(CC) $(INCLUDE) -shared -O2 -Wall -Xlinker -x -o mod_g729.so  \
	   	mod_g729.o libg729.o decg729.o encg729.o owng729.o vadg729.o aux_tbls.o \
	  	-lm $(LDFLAGS) -L/usr/local/lib -Wl,-Bdynamic $(FSBASE)/.libs/libfreeswitch.so -pthread -lrt -luuid -lcrypt


decg729.o : decg729.c
	$(CC) $(CC_OPT) -c decg729.c $(INCLUDE)
encg729.o : encg729.c
	$(CC) $(CC_OPT) -c encg729.c $(INCLUDE)
owng729.o : owng729.c 
	$(CC) $(CC_OPT) -c owng729.c $(INCLUDE)
vadg729.o : vadg729.c
	$(CC) $(CC_OPT) -c vadg729.c $(INCLUDE)
aux_tbls.o : aux_tbls.c
	$(CC) $(CC_OPT) -c aux_tbls.c $(INCLUDE)

libg729.o: libg729.c
	$(CC) $(CC_OPT) -c libg729.c $(INCLUDE)
mod_g729.o: mod_g729.c
	$(CC) $(CC_OPT) -c mod_g729.c $(INCLUDE)

clean:
	rm *.o
