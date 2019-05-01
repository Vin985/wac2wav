# file: wac2wav.pyx

cdef extern from "c/wac2wav.h":
    int wac2wav_c(char *srcfile, char *destfile)

def wac2wav(src, dest):
    wac2wav_c(bytes(src, "utf-8"), bytes(dest, "utf-8"))

def wacs2wav(list):
    for (src, dest) in list:
      wac2wav_c(bytes(src, "utf-8"), bytes(dest, "utf-8"))
