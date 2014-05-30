/*
    uw2_gr_tool: "Ultima Underworld II" .GR extracter/rebuilder.
    Copyright (C) 2014, Boris I. Bendovsky <bibendovsky@hotmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA. 
*/


// TODO
// - Check dimensions of last panel (panels.gr).


#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif // _WIN32

#include <cassert>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <locale>
#include <map>
#include <sstream>
#include <string>
#include <vector>


namespace {


#ifdef _WIN32
const char k_path_separator = '\\';
#else
const char k_path_separator = '/';
#endif


std::string combine_path(
    const std::string& path1,
    const std::string& path2)
{
    if (path1.empty())
        return path2;

    if (path2.empty())
        return path1;

    if (path2[0] == k_path_separator)
        return path2;

    bool use_separator = (path1[path1.size() - 1] != k_path_separator);

    std::string result(path1);

    if (use_separator)
        result += k_path_separator;

    result += path2;

    return result;
}

std::string combine_path(
    const std::string& path1,
    const std::string& path2,
    const std::string& path3)
{
    return combine_path(combine_path(path1, path2), path3);
}

std::string to_lowercase(
    const std::string& string)
{
    if (string.empty())
        return std::string();

    static std::locale locale;

    static const std::ctype<char>& facet =
        std::use_facet<std::ctype<char> >(locale);

    std::string result(string);

    facet.tolower(&result[0], &result[0] + result.size());

    return result;
}

std::string to_uppercase(
    const std::string& string)
{
    if (string.empty())
        return std::string();

    static std::locale locale;

    static const std::ctype<char>& facet =
        std::use_facet<std::ctype<char> >(locale);

    std::string result(string);

    facet.toupper(&result[0], &result[0] + result.size());

    return result;
}

bool normalize_path_pred(
    char c)
{
#ifdef _WIN32
    return c == '/';
#else
    return c == '\\';
#endif
}

std::string normalize_path(
    const std::string& path)
{
    std::string result(path);

    std::replace_if(
        result.begin(), result.end(), normalize_path_pred, k_path_separator);

    return result;
}

std::string extract_dir(
    const std::string& path)
{
    if (path.empty())
        return std::string();

    size_t name_pos = path.rfind(k_path_separator);

    if (name_pos == path.npos)
        return std::string();

    return path.substr(0, name_pos - 1);
}

std::string extract_file_name(
    const std::string& path)
{
    if (path.empty())
        return std::string();

    size_t name_pos = path.rfind(k_path_separator);

    if (name_pos == (path.size() - 1))
        return std::string();

    if (name_pos == path.npos)
        name_pos = 0;
    else
        ++name_pos;

    return path.substr(name_pos);
}

std::string extract_file_name_without_extension(
    const std::string& path)
{
    if (path.empty())
        return std::string();

    size_t name_pos = path.rfind(k_path_separator);

    if (name_pos == (path.size() - 1))
        return std::string();

    if (name_pos == path.npos)
        name_pos = 0;
    else
        ++name_pos;

    size_t dot_pos = path.rfind('.');

    if (dot_pos == path.npos)
        dot_pos = path.size();

    size_t length = dot_pos - name_pos;
    return path.substr(name_pos, length);
}

bool create_dir(
    const std::string& path)
{
#ifdef _WIN32
    int api_result = ::_mkdir(path.c_str());
#else
    int api_result = ::mkdir(path.c_str(), 0777);
#endif // _WIN32

    if (api_result != 0) {
        if (errno != EEXIST) {
            std::cerr <<
                "ERROR: Failed to create a directory \"" <<
                path << "\"." << std::endl;

            return false;
        }
    }

    return true;
}

bool create_dirs_along_the_path(
    const std::string& path)
{
    if (path.empty())
        return true;

    std::string current_path;
    size_t dir_start_pos = 0;
    size_t dir_end_pos = 0;

    while (dir_end_pos != std::string::npos) {
        dir_end_pos = path.find(k_path_separator, dir_start_pos);

        current_path = combine_path(current_path, path.substr(
            dir_start_pos, dir_end_pos - dir_start_pos));

        if (!create_dir(current_path))
            return false;

        dir_start_pos = dir_end_pos;

        if (dir_start_pos != std::string::npos)
            ++dir_start_pos;
    }

    return true;
}

bool is_file_exists(
    const std::string& file_name)
{
    std::ifstream file(
        file_name.c_str(),
        std::ios_base::in | std::ios_base::binary | std::ios_base::ate);

    return file.is_open();
}

template<typename T>
void write_value(
    T value,
    std::ostream& stream)
{
    stream.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

template<typename T>
void read_value(
    T& value,
    std::istream& stream)
{
    stream.read(reinterpret_cast<char*>(&value), sizeof(T));
}


// ========================================================================


const int k_max_width = 255;
const int k_max_height = 255;

const int k_panel_width = 79;
const int k_panel_height = 112;

const int k_panel_border_width = 3;
const int k_panel_border_height = 112;


typedef std::map<std::string,int> PaletteMap;
typedef std::map<int,std::string> Mappings;
typedef Mappings::iterator MappingsIt;
typedef Mappings::const_iterator MappingsCIt;
typedef std::vector<unsigned char> Buffer;
typedef Buffer Palette;
typedef unsigned char AuxPalette[16];
typedef AuxPalette AuxPalettes[32];


class BmpHeader {
public:
    unsigned short bfType;
    unsigned long bfSize;
    unsigned short bfReserved1;
    unsigned short bfReserved2;
    unsigned long bfOffBits;

    void save_to_stream(
        std::ostream& stream)
    {
        write_value(bfType, stream);
        write_value(bfSize, stream);
        write_value(bfReserved1, stream);
        write_value(bfReserved2, stream);
        write_value(bfOffBits, stream);
    }

    void load_from_stream(
        std::istream& stream)
    {
        read_value(bfType, stream);
        read_value(bfSize, stream);
        read_value(bfReserved1, stream);
        read_value(bfReserved2, stream);
        read_value(bfOffBits, stream);
    }

    static int get_size()
    {
        return 14;
    }
}; // class BmpHeader

class BmpInfoHeader {
public:
    enum Compression {
        e_rgb = 0, // BI_RGB
        e_rle8 = 1 // BI_RLE8
    }; // enum Compression

    unsigned long biSize;
    long biWidth;
    long biHeight;
    unsigned short biPlanes;
    unsigned short biBitCount;
    unsigned long biCompression;
    unsigned long biSizeImage;
    long biXPelsPerMeter;
    long biYPelsPerMeter;
    unsigned long biClrUsed;
    unsigned long biClrImportant;

    void save_to_stream(
        std::ostream& stream)
    {
        write_value(biSize, stream);
        write_value(biWidth, stream);
        write_value(biHeight, stream);
        write_value(biPlanes, stream);
        write_value(biBitCount, stream);
        write_value(biCompression, stream);
        write_value(biSizeImage, stream);
        write_value(biXPelsPerMeter, stream);
        write_value(biYPelsPerMeter, stream);
        write_value(biClrUsed, stream);
        write_value(biClrImportant, stream);
    }

    void load_from_stream(
        std::istream& stream)
    {
        read_value(biSize, stream);
        read_value(biWidth, stream);
        read_value(biHeight, stream);
        read_value(biPlanes, stream);
        read_value(biBitCount, stream);
        read_value(biCompression, stream);
        read_value(biSizeImage, stream);
        read_value(biXPelsPerMeter, stream);
        read_value(biYPelsPerMeter, stream);
        read_value(biClrUsed, stream);
        read_value(biClrImportant, stream);
    }

    bool is_compressed() const
    {
        return biCompression != e_rgb;
    }

    static int get_size()
    {
        return 40;
    }
}; // class BmpInfoHeader

class NibbleReader {
public:
    NibbleReader(
        const unsigned char* data,
        int data_size) :
            nibble_index_(2),
            data_(data),
            data_size_(data_size),
            data_offset_()
    {
        assert(data);
        assert(data_size >= 0);
    }

    ~NibbleReader()
    {
    }

    unsigned char read()
    {
        if (data_offset_ == data_size_)
            return 0;

        if (nibble_index_ == 2) {
            unsigned char octet = data_[data_offset_];
            nibble_buffer_[0] = octet >> 4;
            nibble_buffer_[1] = octet & 0x0F;
            nibble_index_ = 0;
            ++data_offset_;
        }

        unsigned char result = nibble_buffer_[nibble_index_];

        ++nibble_index_;

        return result;
    }

private:
    int nibble_index_;
    unsigned char nibble_buffer_[2];
    const unsigned char* data_;
    int data_size_;
    int data_offset_;

    NibbleReader(
        const NibbleReader& that);

    NibbleReader& operator=(
        const NibbleReader& that);
}; // class NibbleReader

class Bitmap {
public:
    enum Special {
        e_none,
        e_default,
        e_panel,
        e_last_panel
    }; // enum Special

    int type;
    int width;
    int height;

    // If type is 4 the size in bytes otherwise in nibbles.
    int data_size;

    Special special;
    Buffer pixels;
    const Palette* palette;
    const AuxPalette* aux_palette;

    Bitmap() :
        type(),
        width(),
        height(),
        data_size(),
        special(),
        pixels(),
        palette(),
        aux_palette()
    {
    }

    Bitmap(
        const Bitmap& that) :
            type(that.type),
            width(that.width),
            height(that.height),
            data_size(that.data_size),
            special(that.special),
            pixels(that.pixels),
            palette(that.palette),
            aux_palette(that.aux_palette)
    {
    }

    Bitmap& operator=(
        const Bitmap& that)
    {
        if (&that != this) {
            type = that.type;
            width = that.width;
            height = that.height;
            data_size = that.data_size;
            special = that.special;
            pixels = that.pixels;
            palette = that.palette;
            aux_palette = that.aux_palette;
        }

        return *this;
    }

    ~Bitmap()
    {
    }

    bool load_from_gr(
        const void* data,
        Special special,
        const Palette* palette,
        const AuxPalettes& aux_palette)
    {
        assert(data);
        assert(palette);

        const unsigned char* octets = static_cast<const unsigned char*>(data);

        if (special == e_default) {
            type = octets[0];
            width = octets[1];
            height = octets[2];
            octets += 3;
        } else {
            type = 4;

            if (special == e_last_panel) {
                width = k_panel_border_width;
                height = k_panel_border_height;
            } else {
                width = k_panel_width;
                height = k_panel_height;
            }

            data_size = width * height;
        }

        switch (type) {
        case 4:
        case 8:
        case 10:
            break;

        default:
            std::cerr << "ERROR: Invalid bitmap type: " <<
                type << '.' << std::endl;
            return false;
        }

        if (is_compressed()) {
            int aux_palette_index = *octets++;

            if (aux_palette_index > 31) {
                std::cerr << "ERROR: Auxiliary palette index out of range: " <<
                    aux_palette_index << '.' << std::endl;
                return false;
            }

            this->aux_palette = &aux_palette[aux_palette_index];
        } else
            this->aux_palette = NULL;

        if (special == e_none || special == e_default) {
            data_size = *reinterpret_cast<const unsigned short*>(&octets[0]);
            octets += 2;
        }

        int size_in_bytes = get_size_in_bytes();

        pixels.clear();
        pixels.resize(size_in_bytes);

        std::uninitialized_copy(
            &octets[0],
            &octets[size_in_bytes],
            &pixels[0]);

        this->palette = palette;

        return true;
    }

    void decompress(
        Buffer& buffer) const
    {
        if (!is_compressed()) {
            buffer = pixels;
            return;
        }

        buffer.clear();

        if (pixels.empty())
            return;

        buffer.resize(width * height);

        if (type == 8) {
            NibbleReader reader(
                &pixels[0],
                get_size_in_bytes());

            int buffer_offset = 0;

            int pixel_count = 0;
            int stage = 0; // we start in stage 0
            int count = 0;
            int record = 0; // we start with record 0=repeat (3=run)
            int repeat_count = 0;

            int data_length = data_size;
            int area = width * height;

            while (data_length > 0 && pixel_count < area) {
                int nibble = reader.read();

                --data_length;

                switch (stage) {
                case 0: // we retrieve a new count
                    if (nibble == 0)
                        ++stage;
                    else {
                        count = nibble;
                        stage = 6;
                    }
                    break;

                case 1:
                    count = nibble;
                    ++stage;
                    break;

                case 2:
                    count = (count << 4) | nibble;

                    if (count == 0)
                        ++stage;
                    else
                        stage = 6;
                    break;

                case 3:
                case 4:
                case 5:
                    count = (count << 4) | nibble;
                    ++stage;
                    break;
                }

                if (stage < 6)
                    continue;

                switch (record) {
                case 0:
                    // repeat record stage 1

                    if (count == 1) {
                        // skip this record; a run follows
                        record = 3;
                        break;
                    }

                    if (count == 2) {
                        // multiple run records
                        record = 2;
                        break;
                    }

                    // read next nibble; it's the color to repeat
                    record = 1;
                    continue;

                case 1:
                    // repeat record stage 2

                    // repeat 'nibble' color 'count' times
                    for (int n = 0; n < count; ++n) {
                        buffer[buffer_offset++] = (*aux_palette)[nibble];

                        if (++pixel_count >= area)
                            break;
                    }

                    if (repeat_count == 0)
                        record = 3; // next one is a run record
                    else {
                        --repeat_count;
                        record = 0; // continue with repeat records
                    }
                    break;

                case 2:
                    // multiple repeat stage
                    // 'count' specifies the number of repeat record to appear
                    repeat_count = count - 1;
                    record = 0;
                    break;

                case 3:
                    // run record stage 1
                    // copy 'count' nibbles

                    // retrieve next nibble
                    record = 4;
                    continue;

                case 4:
                    // run record stage 2

                    // now we have a nibble to write
                    buffer[buffer_offset++] = (*aux_palette)[nibble];
                    ++pixel_count;

                    if (--count == 0)
                        record = 0; // next one is a repeat again
                    else
                        continue;
                    break;
                }

                stage = 0;
            }
        }

        if (type == 10) {
            // 4-bit uncompressed

            NibbleReader reader(&pixels[0], data_size);

            for (int i = 0; i < data_size; ++i)
                buffer[i] = reader.read();
        }
    }

    bool export_to_bmp(
        const std::string& file_name) const
    {
        std::cout << "Exporting a bitmap to \"" <<
            file_name << "\"." << std::endl;

        std::ofstream file(
            file_name.c_str(), std::ios_base::out | std::ios_base::binary);

        if (!file) {
            std::cerr << "ERROR: Unable to open." << std::endl;
            return false;
        }

        int pad = (((width + 3) / 4) * 4) - width;

        Buffer bmp_color_indices;
        decompress(bmp_color_indices);

        BmpHeader header = BmpHeader();
        header.bfType = 0x4D42;
        header.bfSize =
            BmpHeader::get_size() + BmpInfoHeader::get_size() +
            (4 * 256) + ((width + pad) * height);
        header.bfOffBits =
            BmpHeader::get_size() + BmpInfoHeader::get_size() + (4 * 256);

        BmpInfoHeader info_header = BmpInfoHeader();
        info_header.biSize = BmpInfoHeader::get_size();
        info_header.biWidth = width;
        info_header.biHeight = -height;
        info_header.biPlanes = 1;
        info_header.biBitCount = 8;
        info_header.biCompression = 0; // BI_RGB
        info_header.biSizeImage = (width + pad) * height;

        Buffer bmp_palette(1024);

        for (int i = 0; i < 256; ++i) {
            bmp_palette[(4 * i) + 0] = static_cast<unsigned char>(
                (*palette)[(3 * i) + 2] * 255.0F / 63.0F);

            bmp_palette[(4 * i) + 1] = static_cast<unsigned char>(
                (*palette)[(3 * i) + 1] * 255.0F / 63.0F);

            bmp_palette[(4 * i) + 2] = static_cast<unsigned char>(
                (*palette)[(3 * i) + 0] * 255.0F / 63.0F);

            bmp_palette[(4 * i) + 3] = 0;
        }

        header.save_to_stream(file);
        info_header.save_to_stream(file);

        file.write(reinterpret_cast<const char*>(&bmp_palette[0]), 4 * 256);

        if (pad == 0) {
            file.write(
                reinterpret_cast<const char*>(&bmp_color_indices[0]),
                bmp_color_indices.size());
        } else {
            char padding[3];

            for (int i = 0; i < height; ++i) {
                file.write(
                    reinterpret_cast<const char*>(
                        &bmp_color_indices[i * width]),
                    width);

                file.write(padding, pad);
            }
        }

        if (!file) {
            std::cerr << "ERROR: I/O error." << std::endl;
            return false;
        }

        return true;
    }

    bool import_from_bmp(
        const std::string& file_name,
        Special special)
    {
        std::cout << "Importing bitmap from \"" <<
            file_name << "\"." << std::endl;

        std::ifstream file(
            file_name.c_str(),
            std::ios_base::in | std::ios_base::binary);

        if (!file) {
            std::cerr << "ERROR: Failed to open." << std::endl;
            return false;
        }

        //
        BmpHeader header;
        header.load_from_stream(file);

        if (!file) {
            std::cerr << "ERROR: I/O error." << std::endl;
            return false;
        }

        if (header.bfType != 0x4D42) {
            std::cerr << "ERROR: Not a BMP file." << std::endl;
            return false;
        }

        //
        BmpInfoHeader info_header;
        info_header.load_from_stream(file);

        if (!file) {
            std::cerr << "ERROR: I/O error." << std::endl;
            return false;
        }

        if (static_cast<int>(info_header.biSize) < BmpInfoHeader::get_size()) {
            std::cerr << "ERROR: Info header is too small." << std::endl;
            return false;
        }

        if (info_header.biWidth == 0 || info_header.biHeight == 0) {
            std::cerr << "ERROR: Empty image." << std::endl;
            return false;
        }

        if (::abs(info_header.biWidth) > k_max_width) {
            std::cerr << "ERROR: Width is too big." << std::endl;
            return false;
        }

        if (::abs(info_header.biHeight) > k_max_height) {
            std::cerr << "ERROR: Height is too big." << std::endl;
            return false;
        }

        if (info_header.biPlanes != 1) {
            std::cerr << "ERROR: Unsupported number of bitplanes: " <<
                info_header.biPlanes << '.' << std::endl;
            return false;
        }

        if (info_header.biBitCount != 8) {
            std::cerr << "ERROR: Color bit depth is not 8 bit." << std::endl;
            return false;
        }

        switch (info_header.biCompression) {
        case BmpInfoHeader::e_rgb:
        case BmpInfoHeader::e_rle8:
            break;

        default:
            std::cerr << "ERROR: Unsupported compression mode: " <<
                info_header.biCompression << '.' << std::endl;
            return false;
        }

        if (info_header.is_compressed() && info_header.biSizeImage == 0) {
            std::cerr << "ERROR: Unknown size of compressed data." << std::endl;
            return false;
        }

        if (info_header.biClrUsed != 0 && info_header.biClrUsed != 256) {
            std::cerr << "ERROR: Invalid size of palette." << std::endl;
            return false;
        }

        //
        Buffer data(info_header.biSizeImage);
        file.seekg(header.bfOffBits);
        file.read(reinterpret_cast<char*>(&data[0]), data.size());

        if (!file) {
            std::cerr << "ERROR: I/O error." << std::endl;
            return false;
        }

        int width = info_header.biWidth;
        int height = ::abs(info_header.biHeight);

        if (this->width != width || this->height != height) {
            std::cerr <<
                "ERROR: Mismatch dimensions of a new image and an original one." <<
                std::endl;
            return false;
        }

        bool is_top_bottom = (info_header.biHeight < 0);
        int x = 0;
        int y = is_top_bottom ? 0 : height - 1;
        int max_y = is_top_bottom ? height : 0;
        int y_step = is_top_bottom ? 1 : -1;

        pixels.clear();
        pixels.resize(width * height);

        if (info_header.is_compressed()) {
            // Decode RLE8

            bool align = false;
            int count = 0;
            int src_offset = 0;
            unsigned char pixel = 0;
            RleState state = e_rle_repeat;

            while (state != e_rle_finished) {
                switch (state) {
                case e_rle_repeat: {
                    count = data[src_offset++];

                    if (count == 0)
                        state = e_rle_escape;
                    else {
                        align = ((count % 2) != 0);
                        pixel = data[src_offset++];
                        state = e_rle_repeat_write;
                    }
                    break;
                }

                case e_rle_repeat_write:
                    pixels[(y * width) + x] = pixel;

                    ++x;
                    --count;

                    if (count == 0)
                        state = e_rle_repeat;
                    break;

                case e_rle_absolute_write:
                    pixels[(y * width) + x] = data[src_offset++];

                    ++x;
                    --count;

                    if (count == 0) {
                        if (align)
                            state = e_rle_align;
                        else
                            state = e_rle_repeat;
                    }
                    break;

                case e_rle_escape:
                    count = data[src_offset++];

                    switch (count) {
                    case 0:
                        state = e_rle_repeat;
                        break;

                    case 1:
                        state = e_rle_finished;
                        break;

                    case 2:
                        x += data[src_offset++];
                        y += y_step * data[src_offset++];
                        state = e_rle_repeat;
                        break;

                    default:
                        align = ((count % 2) != 0);
                        state = e_rle_absolute_write;
                        break;
                    }

                    break;

                case e_rle_align:
                    ++src_offset;
                    state = e_rle_repeat;
                    break;

                case e_rle_finished:
                    break;
                }

                if (x == width) {
                    x = 0;
                    y += y_step;
                }
            }
        } else {
            int stride = ((width + 3) / 4) * 4;
            int src_offset = 0;

            while (y != max_y) {
                unsigned char* line = &pixels[y * width];

                std::uninitialized_copy(
                    &data[src_offset],
                    &data[src_offset] + width,
                    line);

                src_offset += stride;

                y += y_step;
            }
        }

        type = 4;
        this->width = width;
        this->height = height;
        this->special = special;
        data_size = width * height;
        aux_palette = NULL;

        return true;
    }

    bool is_empty() const
    {
        return pixels.empty();
    }

    bool is_compressed() const
    {
        return type != 4;
    }

    int get_size_in_bytes() const
    {
        if (is_compressed())
            return (data_size + 1) / 2;
        else
            return data_size;
    }

private:
    enum RleState {
        e_rle_repeat,
        e_rle_repeat_write,
        e_rle_absolute_write,
        e_rle_escape,
        e_rle_align,
        e_rle_finished
    }; // enum RleState
}; // class Bitmap

typedef std::vector<Bitmap> Bitmaps;
typedef Bitmaps::iterator BitmapsIt;
typedef Bitmaps::const_iterator BitmapsCIt;

typedef std::vector<Palette> Palettes;


// Globals.
//

const int k_max_file_size = 1 * 1024 * 1024;
const int k_max_palette_count = 8;
const std::string k_mappings_file_name_suffix = "_mappings.txt";


bool g_is_panels;
std::string g_original_file_name;
std::string g_original_base_name_lc;
std::string g_command;
std::string g_path_to_data;
std::string g_in_file_name;
std::string g_out_file_name;
std::string g_in_dir;
std::string g_out_dir;
Mappings g_mappings;
Bitmaps g_bitmaps;
PaletteMap g_palette_map;
Palettes g_palettes;
AuxPalettes g_aux_palettes;
std::string g_user_answer;


bool compare_ci_partialy(
    const std::string& match,
    const std::string& string)
{
    if (string.empty() || match.empty())
        return false;

    if (match.size() > string.size())
        return false;

    size_t size = std::min(string.size(), match.size());

    size_t i;

    for (i = 0; i < size; ++i) {
        if (string[i] != match[i])
            break;
    }

    return i == size;
}

void test_file_for_overwrite(
    const std::string& file_name)
{
    if (g_user_answer == "all" || g_user_answer == "cancel")
        return;

    if (!is_file_exists(file_name))
        return;

    std::string answer;

    for (bool done = false; !done; ) {
        std::cout << "File \"" << file_name <<
            "\" already exist. Overwrite? (all/yes/no/cancel) ";
        std::cin >> answer;

        if (answer.empty())
            continue;

        to_lowercase(answer);

        if (compare_ci_partialy(answer, "all"))
            g_user_answer = "all";
        else if (compare_ci_partialy(answer, "yes"))
            g_user_answer = "yes";
        else if (compare_ci_partialy(answer, "no"))
            g_user_answer = "no";
        else if (compare_ci_partialy(answer, "cancel")) {
            g_user_answer = "cancel";
            std::cout << "Canceled by user." << std::endl;
        }

        if (!g_user_answer.empty())
            break;
    }
}

void initialize_palette_map(
    PaletteMap& palette_map)
{
    palette_map["3DWIN.GR"] = 0;
    palette_map["ANIMO.GR"] = 0;
    palette_map["ARMOR_F.GR"] = 0;
    palette_map["ARMOR_M.GR"] = 0;
    palette_map["BODIES.GR"] = 0;
    palette_map["BUTTONS.GR"] = 0;
    palette_map["CHAINS.GR"] = 0;
    palette_map["CHARHEAD.GR"] = 0;
    palette_map["CHRBTNS.GR"] = 3;
    palette_map["COMPASS.GR"] = 0;
    palette_map["CONVERSE.GR"] = 0;
    palette_map["CURSORS.GR"] = 0;
    palette_map["DOORS.GR"] = 0;
    palette_map["DRAGONS.GR"] = 0;
    palette_map["EYES.GR"] = 0;
    palette_map["FLASKS.GR"] = 0;
    palette_map["GEMPT.GR"] = 0;
    palette_map["GENHEAD.GR"] = 0;
    palette_map["GHED.GR"] = 0;
    palette_map["HEADS.GR"] = 0;
    palette_map["INV.GR"] = 0;
    palette_map["LFTI.GR"] = 0;
    palette_map["OBJECTS.GR"] = 0;
    palette_map["OPBTN.GR"] = 2;
    palette_map["OPTB.GR"] = 0;
    palette_map["OPTBTNS.GR"] = 0;
    palette_map["PANELS.GR"] = 0;
    palette_map["POWER.GR"] = 0;
    palette_map["QUESTION.GR"] = 0;
    palette_map["SCRLEDGE.GR"] = 0;
    palette_map["SPELLS.GR"] = 0;
    palette_map["TMFLAT.GR"] = 0;
    palette_map["TMOBJ.GR"] = 0;
    palette_map["VIEWS.GR"] = 0;
    palette_map["WEAP.GR"] = 0;
}

bool load_palettes(
    const std::string& path,
    Palettes& palettes,
    AuxPalettes& aux_palettes)
{
    std::string file_name;

    //
    file_name = combine_path(path, "PALS.DAT");
    std::cout << "Loading palettes from \"" << file_name << "\"." << std::endl;

    std::ifstream file(
        file_name.c_str(),
        std::ios_base::in | std::ios_base::binary);

    if (!file) {
        std::cerr << "ERROR: Failed to open." << std::endl;
        return false;
    }

    palettes.resize(k_max_palette_count);

    for (int i = 0; i < k_max_palette_count; ++i) {
        palettes[i].resize(768);
        file.read(reinterpret_cast<char*>(&palettes[i][0]), 768);

        if (!file) {
            std::cerr << "ERROR: I/O error." << std::endl;
            return false;
        }
    }

    //
    file_name = combine_path(path, "ALLPALS.DAT");
    std::cout << "Loading auxiliary palettes from \"" <<
        file_name << "\"." << std::endl;

    std::ifstream aux_file(
        file_name.c_str(),
        std::ios_base::in | std::ios_base::binary);

    if (!aux_file) {
        std::cerr << "ERROR: Failed to open." << std::endl;
        return false;
    }

    aux_file.read(reinterpret_cast<char*>(aux_palettes), 32 * 16);

    if (!aux_file) {
        std::cerr << "ERROR: I/O error." << std::endl;
        return false;
    }

    return true;
}

bool load_mappings(
    const std::string& file_name)
{
    std::cout << "Loading mappings from \"" << file_name << "\"" << std::endl;

    std::ifstream file(file_name.c_str());

    if (!file) {
        std::cerr << "ERROR: Failed to open." << std::endl;
        return false;
    }

    g_mappings.clear();

    while (!file.eof()) {
        int bitmap_index;
        std::string bitmap_file_name;

        file >> bitmap_index;

        if (!file) {
            if (!file.eof()) {
                std::cerr << "ERROR: Invalid bitmap index value." << std::endl;
                return false;
            } else
                return true;
        }

        if (bitmap_index < 0) {
            std::cerr << "ERROR: Negative bitmap index." << std::endl;
            return false;
        }

        file >> bitmap_file_name;

        if (!file) {
            std::cerr << "ERROR: Invalid bitmap file name." << std::endl;
            return false;
        }

        if (g_mappings.find(bitmap_index) != g_mappings.end()) {
            std::cerr << "ERROR: Duplicating bitmap index: " <<
                bitmap_index << '.' << std::endl;
            return false;
        }

        g_mappings[bitmap_index] = bitmap_file_name;
    }

    if (g_mappings.empty()) {
        std::cerr << "ERROR: No records." << std::endl;
        return false;
    }

    return true;
}

bool load_gr_file(
    const std::string& file_name)
{
    std::cout << "Loading \"" << file_name << "\"." << std::endl;

    std::ifstream file(
        file_name.c_str(),
        std::ios_base::in | std::ios_base::binary | std::ios_base::ate);

    if (!file) {
        std::cerr << "ERROR: Failed to open." << std::endl;
        return false;
    }

    std::ifstream::pos_type file_size = file.tellg();

    if (file_size == std::ifstream::pos_type(0)) {
        std::cerr << "ERROR: Empty file." << std::endl;
        return false;
    }

    if (file_size > k_max_file_size) {
        std::cerr << "ERROR: File is too big." << std::endl;
        return false;
    }

    file.seekg(0);

    Buffer buffer(static_cast<size_t>(file_size));

    file.read(
        reinterpret_cast<char*>(&buffer[0]),
        static_cast<size_t>(file_size));

    if (!file) {
        std::cerr << "ERROR: I/O error." << std::endl;
        return false;
    }

    int gr_type = buffer[0];

    if (gr_type != 1) {
        std::cerr << "ERROR: Invalid type: " << gr_type << "\"." <<
            std::endl;
        return false;
    }

    int bitmap_count =
        *reinterpret_cast<const unsigned short*>(&buffer[1]);

    if (bitmap_count == 0) {
        std::cerr << "ERROR: No bitmaps." << std::endl;
        return false;
    }

    g_bitmaps.resize(bitmap_count);
    std::vector<int> offsets(bitmap_count + 1);

    for (int i = 0; i < bitmap_count + 1; ++i)
        offsets[i] = *reinterpret_cast<const unsigned long*>(
            &buffer[3 + (4 * i)]);

    for (int i = 0; i < bitmap_count; ++i) {
        Bitmap& bitmap = g_bitmaps[i];

        int data_size = offsets[i + 1] - offsets[i];

        if (data_size == 0) {
            bitmap.special = Bitmap::e_none;
            bitmap.width = 0;
            bitmap.height = 0;
            Buffer().swap(bitmap.pixels);
            bitmap.aux_palette = NULL;
            continue;
        }

        Bitmap::Special special = Bitmap::e_default;

        if (g_is_panels) {
            if (i == (bitmap_count - 1))
                special = Bitmap::e_last_panel;
            else
                special = Bitmap::e_panel;
        }

        int palette_index = g_palette_map[g_original_file_name];
        Palette& palette = g_palettes[palette_index];

        if (!bitmap.load_from_gr(
            &buffer[offsets[i]],
            special,
            &palette,
            g_aux_palettes))
        {
            return false;
        }

        if (bitmap.type != 4 && palette_index != 0) {
            std::cerr <<
                "ERROR: Non zero palette index for compressed bitmap." <<
                std::endl;
            return false;
        }
    }

    return true;
}

bool save_gr_file(
    const std::string& file_name)
{
    test_file_for_overwrite(file_name);

    if (g_user_answer == "no" || g_user_answer == "cancel")
        return false;

    std::cout << "Saving to \"" << file_name << "\"." << std::endl;

    std::ofstream file(
        file_name.c_str(),
        std::ios_base::out | std::ios_base::binary);

    if (!file) {
        std::cerr << "ERROR: Failed to open." << std::endl;
        return false;
    }

    size_t bitmap_count = g_bitmaps.size();
    size_t offset_count = bitmap_count + 1;
    std::vector<unsigned long> offsets(offset_count);
    unsigned long offset = static_cast<unsigned long>(3 + (4 * offset_count));
    offsets[0] = offset;

    for (size_t i = 0; i < bitmap_count; ++i) {
        const Bitmap& bitmap = g_bitmaps[i];

        if (!bitmap.is_empty()) {
            unsigned long size =
                static_cast<unsigned long>(bitmap.get_size_in_bytes());

            if (!g_is_panels) {
                // type, width, height
                size += 1 + 1 + 1;

                if (bitmap.is_compressed())
                    ++size; // aux. palette index

                // image size
                size += 2;
            }

            offset += size;
        }

        offsets[i + 1] = offset;
    }

    // type
    write_value(static_cast<unsigned char>(1), file);

    // image count
    write_value(static_cast<unsigned short>(bitmap_count), file);

    // image offsets
    for (size_t i = 0; i < offset_count; ++i)
        write_value(offsets[i], file);

    // images
    for (BitmapsCIt i = g_bitmaps.begin(); i != g_bitmaps.end(); ++i) {
        const Bitmap& bitmap = *i;

        if (bitmap.is_empty())
            continue;

        if (!g_is_panels) {
            write_value(static_cast<unsigned char>(bitmap.type), file);
            write_value(static_cast<unsigned char>(bitmap.width), file);
            write_value(static_cast<unsigned char>(bitmap.height), file);

            if (bitmap.is_compressed()) {
                int aux_palette_index = bitmap.aux_palette - g_aux_palettes;
                write_value(static_cast<unsigned char>(aux_palette_index), file);
            }

            write_value(static_cast<unsigned short>(bitmap.data_size), file);
        }

        file.write(
            reinterpret_cast<const char*>(&bitmap.pixels[0]),
            bitmap.data_size);
    }

    if (!file) {
        std::cerr << "ERROR: I/O error." << std::endl;
        return false;
    }

    return true;
}

bool save_mappings(
    const std::string& file_name)
{
    std::ofstream file(file_name.c_str());

    std::cout << "Saving mappings to \"" << file_name << "\"." << std::endl;

    if (!file) {
        std::cerr << "ERROR: Failed to open." << std::endl;
        return false;
    }

    for (MappingsCIt i = g_mappings.begin(); i != g_mappings.end(); ++i)
        file << i->first << ' ' << i->second << std::endl;

    return true;
}

bool extract_gr_file()
{
    if (!load_gr_file(g_in_file_name))
        return false;

    if (!create_dirs_along_the_path(g_out_dir))
        return false;

    std::ostringstream oss;

    g_mappings.clear();

    std::string mappings_file_name = combine_path(
        g_out_dir, g_original_base_name_lc + k_mappings_file_name_suffix);

    for (size_t i = 0; i < g_bitmaps.size(); ++i) {
        const Bitmap& bitmap = g_bitmaps[i];

        if (bitmap.is_empty())
            continue;

        oss.clear();
        oss.str(std::string());
        oss << std::setfill('0') << std::setw(4) << i;
        std::string map_name =
            g_original_base_name_lc + '_' + oss.str() + ".bmp";
        std::string bitmap_file_name = combine_path(g_out_dir, map_name);

        test_file_for_overwrite(bitmap_file_name);

        if (g_user_answer.empty() ||
            g_user_answer == "all" ||
            g_user_answer == "yes")
        {
            if (!g_bitmaps[i].export_to_bmp(bitmap_file_name))
                return false;
        } else if (g_user_answer == "cancel")
            return false;

        g_mappings[i] = map_name;
    }

    test_file_for_overwrite(mappings_file_name);

    if (g_user_answer.empty() ||
        g_user_answer == "all" ||
        g_user_answer == "yes")
    {
        if (!save_mappings(mappings_file_name))
            return false;
    } else if (g_user_answer == "cancel")
            return false;

    std::cerr << "Extracted " << g_mappings.size() << " bitmaps." << std::endl;

    return true;
}

bool replace_gr_file()
{
    if (!load_gr_file(g_in_file_name))
        return false;

    std::string list_path =
        combine_path(g_in_dir, g_original_base_name_lc + "_mappings.txt");

    if (!load_mappings(list_path))
        return false;

    int bitmap_count = static_cast<int>(g_bitmaps.size());

    for (MappingsCIt i = g_mappings.begin(); i != g_mappings.end(); ++i) {
        int bitmap_index = i->first;

        if (bitmap_index >= bitmap_count) {
            std::cerr << "ERROR: Bitmap index is out of range: " <<
                bitmap_index << '.' << std::endl;
            return false;
        }

        Bitmap& bitmap = g_bitmaps[bitmap_index];
        std::string bitmap_path = combine_path(g_in_dir, i->second);

        Bitmap::Special special = Bitmap::e_default;

        if (g_is_panels) {
            if (bitmap_index < (bitmap_count - 1))
                special = Bitmap::e_panel;
            else
                special = Bitmap::e_last_panel;
        }

        if (!bitmap.import_from_bmp(bitmap_path, special))
            return false;
    }

    if (!save_gr_file(g_out_file_name))
        return false;

    return true;
}

void usage()
{
    std::cout <<
        "Usage: uw2_gr_tool <cmd> arg1 arg2 ..." << std::endl <<
        "  1) extraction:" << std::endl <<
        "     e <in_file> <out_dir>" << std::endl <<
        "       Extracts all bitmaps from file <in_file> into a directory <out_dir>," << std::endl <<
        "       and creates a file in <out_dir> with mappings of a bitmap index to" << std::endl <<
        "       a file name." << std::endl <<
        "     Path to bitmaps in mappings file is relative to directory <out_dir>." << std::endl <<
        "  2) replacing:" << std::endl <<
        "     r <in_file> <in_dir> <out_file>" << std::endl <<
        "     Replaces bitmaps in file <in_file> with a new ones using mappings" << std::endl <<
        "     file in directory <in_dir> and saves it under a new file name <out_file>." << std::endl <<
        "     Path to bitmaps in mappings file is relative to directory <in_dir>." << std::endl <<
        std::endl <<
        "  Format of the file with mappings:" << std::endl <<
        "    <bitmap_index> <file_name_without_path>" << std::endl <<
        "    ..." << std::endl <<
        std::endl <<
        "  Notes:" << std::endl <<
        "  1) For extraction directory <in_file> must contain the following files:" << std::endl <<
        "     ALLPALS.DAT and PALS.DAT." << std::endl <<
        "  2) Supported BMP formats: 8 bit uncompressed or 8 bit RLE compressed." << std::endl <<
        "  3) BMP file name in mappings file should not" << std::endl <<
        "     contain any whitespaces (space, tab, .etc)." << std::endl
    ;
}


} // namespace


int main(
    int argc,
    char* argv[])
{
    std::cout << "\"Ultima Underworld II\" GR extracter/rebuilder." << std::endl <<
    "Copyright (C) 2014, Boris I. Bendovsky <bibendovsky@hotmail.com>" <<
        std::endl << std::endl;

    if (argc < 3) {
        usage();
        return 1;
    }

    // Check a command.
    //
    g_command = argv[1];

    if (g_command.size() != 1) {
        std::cerr << "ERROR: Invalid command." << std::endl;
        return 1;
    }

    if (g_command[0] != 'e' && g_command[0] != 'r') {
        std::cerr << "ERROR: Invalid command." << std::endl;
        return 1;
    }

    if (g_command == "e") {
        if (argc != 4) {
            usage();
            return 1;
        }
    } else {
        if (argc != 5) {
            usage();
            return 1;
        }
    }

    //
    g_in_file_name = normalize_path(argv[2]);

    g_original_file_name =
        to_uppercase(extract_file_name(g_in_file_name));

    g_original_base_name_lc = to_lowercase(
        extract_file_name_without_extension(g_original_file_name));

    g_is_panels = (g_original_file_name == "PANELS.GR");

    initialize_palette_map(g_palette_map);

    if (g_palette_map.find(g_original_file_name) == g_palette_map.end()) {
        std::cerr << "ERROR: UW2 does not have resource \"" <<
            g_original_file_name << "\"." << std::endl;
        return false;
    }

    g_path_to_data = extract_dir(g_in_file_name);

    if (!load_palettes(
        g_path_to_data,
        g_palettes,
        g_aux_palettes))
    {
        return 2;
    }

    //
    if (g_command == "e") {
        g_out_dir = normalize_path(argv[3]);

        if (!extract_gr_file())
            return 2;
    } else {
        g_in_dir = normalize_path(argv[3]);
        g_out_file_name = normalize_path(argv[4]);

        if (!replace_gr_file())
            return 2;
    }

    return 0;
}
