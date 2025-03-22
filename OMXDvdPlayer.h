#pragma once
/*
 * Copyright (C) 2022 by Michael J. Walsh
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with libdvdnav; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <dvdread/dvd_reader.h>
#include <dvdread/ifo_types.h>
#include <vector>
#include <string>
#include <memory>

#include "utils/NoMoveCopy.h"

class OMXReaderDvd;

class OMXDvdPlayer : NoMoveCopy
{
public:
  explicit OMXDvdPlayer(const std::string &filename);
  ~OMXDvdPlayer();

  bool CanChangeTrack(int delta, int &t);
  OMXReaderDvd *OpenTrack(int ct);

  const std::string &GetID() const { return disc_checksum; }
  const std::string &GetTitle() const { return disc_title; }
  void removeCompositeTracks();

  void info_dump();
  static int dvdtime2msec(dvd_time_t *dt);
  static const char* convertLangCode(uint16_t lang);

  typedef struct {
    unsigned int first_sector;
    int blocks;
  } part_info;

  typedef struct {
    int id; // as in the hex number in "Stream #0:2[0x85]:"
    uint16_t lang; // lang code
  } stream_info;

  typedef struct {
    int title_num;
    std::vector<stream_info> audio_streams;
    std::vector<stream_info> subtitle_streams;
    uint32_t palette[16];
  } title_info;

  typedef struct {
    int cell;
    int time;
    bool is_chapter;
  } cell_info;

  typedef struct {
    std::shared_ptr<title_info> title;
    int length;
    std::vector<cell_info> cells;
    std::vector<part_info> parts;
  } track_info;

private:
  std::vector<track_info> tracks;

  void read_title_name();
  void read_disc_checksum();
  void read_disc_serial_number();

  dvd_reader_t *dvd_device;
  dvd_file_t *dvd_track = NULL;

  int track_no = -1;

  std::string device_path;
  std::string disc_title;
  std::string disc_checksum;

  int yvu2rgb(int c);
};
