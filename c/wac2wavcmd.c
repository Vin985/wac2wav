//////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2014-2017 Wildlife Acoustics, Inc.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// Wildlife Acoustics, Inc.
// 3 Mill and Main Place, Suite 210
// Maynard, MA 01754-2657
// +1 978 369 5225
// www.wildlifeacoustics.com
//
//////////////////////////////////////////////////////////////////////////////

//
// wac2wavcmd.c VERSION 1.0
//
// This code will take an untriggered WAC file (version 4 or earlier) as
// standard input and produce an uncompressed WAV file as standard output.
//
// This code serves as an example of how to decode the Wildlife Acoustics
// proprietary audio compression format known as "WAC".
//
// See comments to learn how GPS data, triggers, and recording tags may be
// interleaved in the data stream.
//
// The comments below are intended to provide some description of how the
// WAC format is implemented, but you should refer to the code as authoratative.
//
// A WAC file has the following format.  Note multi-byte values are
// little-endian.
//
//   1. WAC HEADER (24 bytes)
//      0x00 - 0x03 = "WAac" - identifies this as a WAC file
//      0x04        = Version number (<= 4)
//      0x05        = Channel count (1 for mono, 2 for stereo)
//      0x06 - 0x07 = Frame size = # samples per channel per frame
//      0x08 - 0x09 = Block size = # frames per block
//      0x0a - 0x0b = Flags
//                      0x0f = mask of "lossy bits".  For "WAC0", this is 0.
//                             For "WAC1", this is 1, and so on, representing
//                             increasing levels of compression as the number
//                             of discarded least-significant bits.  WAC0 is
//                             lossless compression, WAC1 is equivalent to
//                             15-bit dynamic range, WAC2 is equivalent to
//                             14-bit dynamic range, and so on.
//                      0x10 = Triggered WAC file e.g. one or both channels
//                             have triggers.  What this means is that highly
//                             compressed zero-value frames may be inserted
//                             in the data stream representing untriggered
//                             time between triggered recordings.  Software
//                             capable of handling triggers can break a file
//                             into pieces discarding these zero-value frames.
//                             Note: This example code does not support
//                             this, but look in the code for "ZERO FRAME"
//                             to see where this would be used.
//                      0x20 = GPS data present - GPS data is interleaved with
//                             data in block headers.
//                      0x40 = TAG data present - TAG data is interleaved with
//                             data in block headers.  The TAG corresponds to
//                             an EM3/EM3+ button press to tag a recording.
//      0x0c - 0x0f = Sample rate (samples per second)
//      0x10 - 0x13 = Sample count (number of samples in WAC file per channel)
//      0x14 - 0x15 = Seek size (number of blocks per seek table entry)
//      0x16 - 0x17 = Seek entries (size of seek table in 32-bit words)
//
// 2. SEEK TABLE
//    The Seek Table contains (Seek entries) number of 4-byte (32-bit)
//    values representing the absolute offset into the WAC file corresponding
//    to each (Seek size) blocks.  The offset is measured in 16-bit words so
//    you would double these values to convert to a byte offset into the file.
//    The intention of the seek table is to make it easier to jump to a position
//    in the WAC file without needing to decompress all the data before that
//    position.  This code example does not use the seek table so we simply
//    skip over it.
//
// 3. BLOCKS OF FRAMES OF SAMPLES
//    Samples are grouped into frames (according to the frame size), and
//    frames are organized into blocks (according to the block size).
//    Additionally, blocks are organized into seek table entries as described
//    above according to the seek size.
//
//    Each block is aligned to a 16-bit boundary and consists of a block
//    header followed by block size frames. The format of the block header is
//    as follows:
//
//    0x00 - 0x03 = 0x00018000 = unique block header pattern
//    0x04 - 0x07 = block index (starting with zero and incrementing by one
//                  for each subsequent block used to keep things synchronized
//                  and detect file corruption.  This is also convenient for
//                  seeking to a particular block as the patterns here will
//                  not occur in the data stream.
//
//    Following the block header are a series of variable-length bit-fields
//    which do not necessarily line up on byte boundaries.  Refer to the
//    ReadBits() function for specifics relating to the encoding.
//
//    If (flags & 0x20), then GPS data is present in every seek size blocks
//    beginning with the first block at index zero.  The GPS data is encoded as
//    25-bits of signed latitude and 26-bits of signed longitude information.
//    (using 2's complement notation). The latitude and longitude values in
//    degrees can be determined by dividing these signed quantities by 100,000
//    with positive values corresponding to North latitude and West longitude.
//
//    If (flags & 0x40), then tag data is present in every block and is
//    represented by 4-bits.  For tagged recordings (e.g. from an EM3), the
//    tag values 1-4 correspond to the buttons 'A' through 'D', and a value 0
//    indicates that no tag is present.  While the tag button is pressed,
//    blocks will be written with the corresponding tag.
//
//    Note that the GPS and TAG values are not implemented in this code and are
//    simply skipped, but please see the comments in the code for more
//    information.
//
//    Following the block header and optional GPS or tag data are block size
//    frames of frame size samples for each channel.  For multi-channel
//    recordings, samples are interleaved.
//
//    Compression uses Golumb coding of the deltas between successive samples.
//    The number of bits used to represent the remainder is variable and
//    optimized for each frame and for each channel.  The quotient is
//    represented by alternating 1/0 bits ahead of the remainder.
//
//    The frame begins with a 4-bit value for each channel indicating the
//    number of bits used to represent the remainder.  Note that a zero value
//    indicates that the frame contains no content e.g. representing the
//    space inbetween triggered recordings.
//
//    What follows are Golumb-encoded representations of deltas of interleaved
//    (by channel) samples.  Details can be found in FrameDecode().
//
//    NOTE: We have not yet added Wildlife Acoustics metadata to the WAC
//    format and may do so in the future, quite likely by appending a
//    "Wamd" chunk at the end of the file.
//
//    NOTE: This code compiles on Linux and should be easy to port to other
//    applications.  Little-endian is assumed.
//
#include "wac2wav.h"

// Simply take stdin to stdout
int main(int argc, char **argv)
{

  char *srcfile = argv[1];
  char *destfile = argv[2];
  fprintf(stderr, "src: %s, dest: %s \n", srcfile, destfile);
  return(wac2wav_c(srcfile, destfile));
}
