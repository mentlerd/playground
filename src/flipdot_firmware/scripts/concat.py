#!/usr/bin/env python3

import argparse
import sys

if __name__ == '__main__':
	parser = argparse.ArgumentParser(description='Concatenate binary files')
	parser.add_argument('dest', type=argparse.FileType('wb'))
	parser.add_argument('src', type=argparse.FileType('rb'), nargs='+')

	args = parser.parse_args()

	for src in args.src:
		args.dest.write(src.read())

	args.dest.flush()
	sys.exit()
