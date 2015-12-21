/*
 FLIF - Free Lossless Image Format
 Copyright (C) 2010-2015  Jon Sneyers & Pieter Wuille, LGPL v3+

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU Lesser General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <stdio.h>
#include <string.h>

class FileIO
{
private:
    FILE *file;
    const char *name;
	
	// prevent copy
	FileIO(const FileIO&) {}
	void operator=(const FileIO&) {}
	// prevent move, for now
	FileIO(FileIO&&) {}
	void operator=(FileIO&&) {}
public:
    const int EOS = EOF;

    FileIO(FILE* fil, const char *aname) : file(fil), name(aname) { }
    ~FileIO() {
        if (file) fclose(file);
    }
    void flush() {
        fflush(file);
    }
    bool isEOF() {
      return feof(file);
    }
    long ftell() {
      return ::ftell(file);
    }
    int getc() {
      return fgetc(file);
    }
    char * gets(char *buf, int n) {
      return fgets(buf, n, file);
    }
    int fputs(const char *s) {
      return ::fputs(s, file);
    }
    int fputc(int c) {
      return ::fputc(c, file);
    }
    void fseek(long offset, int where) {
      ::fseek(file, offset,where);
    }
    const char* getName() {
      return name;
    }
};

/*!
 * Read-only IO interface for a constant memory block
 */
class BlobReader
{
private:
    const uint8_t* data;
    size_t data_array_size;
    size_t seek_pos;
public:
    const int EOS = -1;

    BlobReader(const uint8_t* _data, size_t _data_array_size)
    : data(_data)
    , data_array_size(_data_array_size)
    , seek_pos(0)
    {
    }

    bool isEOF() const {
        return seek_pos >= data_array_size;
    }
    long ftell() const {
        return seek_pos;
    }
    int getc() {
        if(seek_pos >= data_array_size)
            return EOS;
        return data[seek_pos++];
    }
    char * gets(char *buf, int n) {
        int i = 0;
        const int max_write = n-1;
        while(seek_pos < data_array_size && i < max_write)
            buf[i++] = data[seek_pos++];
        buf[n-1] = '\0';

        if(i < max_write)
            return 0;
        else
            return buf;
    }
    int fputc(int c) {
      // cannot write on const memory
      return EOS;
    }
    void fseek(long offset, int where) {
        switch(where) {
        case SEEK_SET:
            seek_pos = offset;
            break;
        case SEEK_CUR:
            seek_pos += offset;
            break;
        case SEEK_END:
            seek_pos = long(data_array_size) + offset;
            break;
        }
    }
    static const char* getName() {
        return "BlobReader";
    }
};

/*!
 * IO interface for a growable memory block
 */
class BlobIO
{
private:
    uint8_t* data;
    // keeps track how large the array really is
    // HINT: should only be used in the grow() function
    size_t data_array_size;
    // keeps track how many bytes were written
    size_t bytes_used;
    size_t seek_pos;

    void grow(size_t necessary_size) {
        if(necessary_size < data_array_size)
            return;

        size_t new_size = necessary_size;
        // start with reasonably large array
        if(new_size < 4096)
            new_size = 4096;

        if(new_size < data_array_size * 3 / 2)
            new_size = data_array_size * 3 / 2;
        uint8_t* new_data = new uint8_t[new_size];

        memcpy(new_data, data, bytes_used);
        // if seek_pos has been moved beyond the written bytes,
        // fill the empty space with zeroes
        for(size_t i = bytes_used; i < seek_pos; ++i)
            new_data[i] = 0;
        delete [] data;
        data = new_data;
        std::swap(data_array_size, new_size);
    }
public:
    const int EOS = -1;

    BlobIO()
    : data(0)
    , data_array_size(0)
    , bytes_used(0)
    , seek_pos(0)
    {
    }

    ~BlobIO() {
        delete [] data;
    }

    uint8_t* release(size_t* array_size)
    {
        uint8_t* ptr = data;
        *array_size = bytes_used;

        data = 0;
        data_array_size = 0;
        bytes_used = 0;
        seek_pos = 0;
        return ptr;
    }

    static void flush() {
        // nothing to do
    }
    bool isEOF() const {
        return seek_pos >= bytes_used;
    }
    int ftell() const {
        return seek_pos;
    }
    int getc() {
        if(seek_pos >= bytes_used)
            return EOS;
        return data[seek_pos++];
    }
    char * gets(char *buf, int n) {
        int i = 0;
        const int max_write = n-1;
        while(seek_pos < bytes_used && i < max_write)
            buf[i++] = data[seek_pos++];
        buf[n-1] = '\0';

        if(i < max_write)
            return 0;
        else
            return buf;
    }
    int fputs(const char *s) {
        size_t i = 0;
        // null-terminated string
        while(s[i])
        {
            grow(seek_pos + 1);
            data[seek_pos++] = s[i++];
            if(bytes_used < seek_pos)
                bytes_used = seek_pos+1;
        }
        return 0;
    }
    int fputc(int c) {
        grow(seek_pos + 1);

        data[seek_pos++] = static_cast<uint8_t>(c);
        if(bytes_used < seek_pos)
            bytes_used = seek_pos+1;
        return c;
    }
    void fseek(long offset, int where) {
        switch(where) {
        case SEEK_SET:
            seek_pos = offset;
            break;
        case SEEK_CUR:
            seek_pos += offset;
            break;
        case SEEK_END:
            seek_pos = long(bytes_used) + offset;
            break;
        }
    }
    static const char* getName() {
        return "BlobIO";
    }
};

