/*
 * Copyright (C) 2009-2011 by Benedict Paten (benedictpaten@gmail.com)
 *
 * Released under the MIT license, see LICENSE.txt
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <float.h>
#include <ctype.h>
#include <errno.h>

#include "bioioC.h"
#include "commonC.h"

#define MAX_PATH_LENGTH 1024
#define MAX_HEADER_LENGTH 512
#define MAX_LINE_LENGTH 80

/* Need to keep data global so it can be accessed by callback
 * given to fastaReadToFunction()
 */
static char curName[MAX_HEADER_LENGTH + 1] = "\0";
static int32_t curOffset = -1;
static const char* curSequence = NULL;
static int32_t curLength = -1;

static char prevName[MAX_HEADER_LENGTH + 1] = "\0";
static int32_t prevOffset = -1;
static const char* prevSequence = NULL;
static int32_t prevLength = -1;

static FILE* outputFile = NULL;


/* Replace prev with cur (if it was not entirely inerted within current)
 */
static void setPrevToCur()
{
	if (strcmp(prevName, curName) == 0)
	{
		if (curOffset + curLength > prevOffset + prevLength)
		{
			prevOffset = curOffset;
			prevSequence = curSequence;
			prevLength = curLength;
		}
	}
	else
	{
		strcpy(prevName, curName);
		prevOffset = curOffset;
		prevSequence = curSequence;
		prevLength = curLength;
	}
}

/* Return lower case value if either c1 or c2 is lower case
 */
static char mergeCharacters(char c1, char c2)
{
	char lc1 = tolower(c1);
	char lc2 = tolower(c2);

	assert(lc1 == lc2);

	if (c1 == lc1 || c2 == lc2)
	{
		return lc2;
	}

	return c2;
}

/* seek back over whitespace.  end on first non-whitespace character */
static void skipBackWhitespace()
{
	char c = getc(outputFile);
	while (!isalnum(c))
	{
		fseek(outputFile, -2, SEEK_CUR);
		c = getc(outputFile);
	}
	fseek(outputFile, -1, SEEK_CUR);
}

/* seek back over header.  end on character before > */
static void skipBackHeader()
{
	char c = getc(outputFile);
	if (isdigit(c))
	{
		while (c != '>')
		{
			fseek(outputFile, -2, SEEK_CUR);
			c = getc(outputFile);
		}
		fseek(outputFile, -1, SEEK_CUR);
	}
	fseek(outputFile, -1, SEEK_CUR);
}

static void skipBack()
{
	skipBackWhitespace();
	skipBackHeader();
	skipBackWhitespace();
}

/* Overwrite overlapping portion of prev and cur sequence to file,
 * where each base is softmasked if is a repeat in either of the sequences
 */
static void insertCurrent(int32_t startFileOffset, int32_t endFileOffset)
{
	/* would be nice to compute overlap directly and
	 * then write whole thing to file, but newline characters
	 * could make that difficult / risky, so we just do it slowly
	 * char by char for now
	 */

	int32_t i, fs;

	/* start at the end file offset and work back */
	for (i = 0; i > endFileOffset; --i)
	{
		skipBack();
		fs = fseek(outputFile, -1, SEEK_CUR);
		assert(fs == 0);
	}

	/* go through input interval backwards, inserting it into the file */
	for (i = endFileOffset; i > startFileOffset; --i)
	{
		/* step back one place in the file */
		skipBack();

		char prevChar = getc(outputFile);
		fseek(outputFile, -1, SEEK_CUR);

		char curChar = curSequence[i - startFileOffset - 1];
		char mergeChar = mergeCharacters(prevChar, curChar);

		putc(mergeChar, outputFile);
		fseek(outputFile, -2, SEEK_CUR);
	}

	/* jump back from end file offset to the end of file */
	fseek(outputFile, 0, SEEK_END);
}

/* Write current sequence (sans header and overlap) to the file
 */
static void appendCurrent(int32_t remaining)
{
	char buffer[MAX_LINE_LENGTH + 1];
	int32_t count = curLength - remaining;
	while (count < curLength)
	{
		int32_t len = curLength - count;
		if (len > MAX_LINE_LENGTH)
		{
			len = MAX_LINE_LENGTH;
		}
		strncpy(buffer, curSequence + count, len);
		buffer[len] = '\0';
		fprintf(outputFile, "%s\n", buffer);
		count += len;
	}
}

/* Merge a sequence (already read into cur*) into the output file
 */
static void mergeChunkIntoFile()
{
	/* New sequence: just copy it in*/
	if (strcmp(curName, prevName) != 0)
	{
		assert(curOffset == 0);
		fastaWrite((char*)curSequence, curName, outputFile);
	}
	else
	{
		/* positions relative to current position in output file stream
		 * where writing needs to take place for the "ins"ert sequence portion
		 * and "app"end sequence portion.
		 */
		int32_t insStartFileOffset = curOffset - prevOffset - prevLength;
		int32_t insEndFileOffset = insStartFileOffset + curLength;
		int32_t remaining = 0;
		if (insEndFileOffset > 0)
		{
			remaining = insEndFileOffset;
			insEndFileOffset = 0;
		}

		assert(insEndFileOffset <= 0);
		assert(insStartFileOffset <= insEndFileOffset);
		assert(remaining >= 0);

		/* step back in file, and merge in overlapped sequence.  */
		if (insStartFileOffset < 0)
		{
			assert(insEndFileOffset > insStartFileOffset);
			insertCurrent(insStartFileOffset, insEndFileOffset);
		}

		/* finish writing the current sequence */
		appendCurrent(remaining);
	}
}

/* Read fasta sequence from file into the "cur" variables.
 * Then merge into the outputFile
 */
static void readFastaCallback(const char *fastaHeader, const char *sequence, int32_t length)
{
	char* oneStr = strstr(fastaHeader, "|1|");
	assert(oneStr != NULL);
	int32_t nameLen = oneStr - fastaHeader;
	assert(nameLen > 0 && nameLen < MAX_HEADER_LENGTH);

	strncpy(curName, fastaHeader, nameLen);
	curName[nameLen] = '\0';

	sscanf(fastaHeader + nameLen + 3, "%d", &curOffset);
	assert(curOffset >= 0);

	curSequence = sequence;
	curLength = length;

	/* merge data from "cur" variables into output file.  "prev" variables
	 * used to detect if overlap is present
	 */
	mergeChunkIntoFile();
	setPrevToCur();
}

int main(int argc, char *argv[])
{
	if (argc != 3)
	{
		fprintf(stderr, "USAGE: %s <file containining list of fasta chunks> \
				<output fasta file>\n\n", argv[0]);
		return -1;
	}

	FILE* chunkListFile = fopen(argv[1], "r");

	if (!chunkListFile)
	{
		fprintf(stderr, "ERROR: cannot open %s for reading", argv[1]);
		return -1;
	}

	/* the + is so we can use fseek() */
	outputFile = fopen(argv[2], "w+");

	if (!outputFile)
	{
		fprintf(stderr, "ERROR: cannot open %s for writing", argv[2]);
		fclose(chunkListFile);
		return -1;
	}

	char chunkFileName[MAX_PATH_LENGTH + 1];
	while (fscanf(chunkListFile, "%s", chunkFileName) == 1)
	{
		/* Read a sequence from the fasta file into the global "cur" variables */
		FILE* chunkFile = fopen(chunkFileName, "r");
		fastaReadToFunction(chunkFile, readFastaCallback);
		fclose(chunkFile);
	}

    fclose(chunkListFile);
    fclose(outputFile);
    return 0;
}
