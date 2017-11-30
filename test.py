#!/usr/bin/env python3
# encoding: utf-8
# filename: profile.py

import wac2wav

DIR_PATH = "../../../data/"
src_name = "Data/IGLOOLIK24_20150620_140000.wac"
dest_name = "wav/test_real.wav"
src_path = DIR_PATH + src_name
dest_path = DIR_PATH + dest_name

wac2wav.wac2wav(src_path, dest_path)
