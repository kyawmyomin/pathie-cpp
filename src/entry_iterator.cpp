/* -*- coding: utf-8 -*-
 * This file is part of Pathie.
 *
 * Copyright © 2015, 2017 Marvin Gülker
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "../include/entry_iterator.hpp"
#include "../include/path.hpp"
#include "../include/errors.hpp"

#if defined(__unix__)
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#elif defined(_WIN32)
#include <Windows.h>
#else
#error Unsupported system
#endif

using namespace Pathie;

/**
 * The default constructor always constructs the terminal
 * iterator, i.e. the one you want to test for if you want
 * to know whether an iteration has completed.
 */
entry_iterator::entry_iterator()
  : mp_directory(NULL),
    mp_cur(NULL),
    mp_cur_path(NULL)
{
}

/**
 * Construct an iterator that reads the entries in the given directory.
 */
entry_iterator::entry_iterator(Path* p_directory)
  : mp_directory(p_directory),
    mp_cur(NULL),
    mp_cur_path(new Path())
{
  open_native_handle();
}

/**
 * Destructor. Closes the open native handle, if it is open.
 */
entry_iterator::~entry_iterator()
{
  close_native_handle();
  // `mp_directory' is NOT deleted, because this class does not own it!
}

/**
 * Opens the native handle to the directory and reads the first
 * entry from the directory.
 */
void entry_iterator::open_native_handle()
{
#if defined(_PATHIE_UNIX)
  std::string nstr = mp_directory->native();
  mp_cur = opendir(nstr.c_str());

  if (mp_cur) {
    struct dirent* p_dirent = readdir(static_cast<DIR*>(mp_cur));
    *mp_cur_path = filename_to_utf8(p_dirent->d_name);
  }
  else {
    throw(Pathie::ErrnoError(errno));
  }
#elif defined(_WIN32)
  std::wstring utf16 = utf8_to_utf16(m_path + "/*");
  WIN32_FIND_DATAW finddata;

  mp_cur = FindFirstFileW(utf16.c_str(), &finddata);
  if (static_cast<HANDLE>(findhandle) == INVALID_HANDLE_VALUE) {
    DWORD err = GetLastError();
    mp_cur = NULL;
    throw(Pathie::WindowsError(err));
  }
  else {
    *mp_cur_path = utf16_to_utf8(finddata.cFileName);
  }
#else
#error Unsupported system
#endif
}

/// Helper function for closing the native handle.
void entry_iterator::close_native_handle()
{
  if (!mp_cur)
    return;

#if defined(_PATHIE_UNIX)
  closedir(static_cast<DIR*>(mp_cur));
#elif defined(_WIN32)
  FindClose(static_cast<HANDLE>(mp_cur));
#endif

  // Reset member variables
  *mp_cur_path = Path();
  mp_cur = NULL;
}

/**
 * Increment operator. Calling this advances the iterator by one,
 * thus pointing it to the next entry. If the end is reached,
 * the iterator will compare equal to the return value of the
 * default constructor, and dereferencing it yields an undefined
 * result.
 */
entry_iterator& entry_iterator::operator++(int)
{
  if (mp_cur) {
#if defined(_PATHIE_UNIX)
    struct dirent* p_dirent = readdir(static_cast<DIR*>(mp_cur));
    if (p_dirent) {
      *mp_cur_path = filename_to_utf8(p_dirent->d_name);
    }
    else {
      close_native_handle();
    }
#elif defined(_WIN32)
    WIN32_FIND_DATAW finddata;
    if (FindNextFileW(static_cast<HANDLE>(mp_cur), &finddata)) {
      *mp_cur_path = utf16_to_utf8(finddata.cFileName);
    }
    else {
      close_native_handle();
    }
#else
#error Unsupported system
#endif
  }
  else { // Finished already
    throw(std::range_error("Tried to advance a finished entry_iterator!"));
  }

  return *this;
}

entry_iterator& entry_iterator::operator++()
{
  return (operator++());
}

/**
 * Derefence operator. Returns the entry the iterator currently
 * points at.
 */
const Path& entry_iterator::operator*() const
{
  return *mp_cur_path;
}

/**
 * Resets this iterator to start again on the path given.
 */
entry_iterator& entry_iterator::operator=(Path* p_directory)
{
  close_native_handle();
  mp_directory = p_directory;
  open_native_handle();
  return *this;
}

/**
 * Boolean operator. In comparisons, this iterator is true if
 * it has not yet finished, false otherwise.
 */
entry_iterator::operator bool() const
{
  return !!mp_directory;
}

bool entry_iterator::operator==(const entry_iterator& other) const
{
  if (other.mp_directory == NULL) {
    /* `mp_directory' is only null for the terminal iterator, that is,
     * a test for the terminal iterator was requested. An entry_iterator
     * is terminated when `mp_cur' is null, so that's what is returned
     * in reality when a test with the terminal iterator is
     * requested. */
    return !!mp_cur;
  }
  else {
    return mp_directory == other.mp_directory && mp_cur == other.mp_cur;
  }
}

bool entry_iterator::operator!=(const entry_iterator& other) const
{
  return !(*this == other);
}

const Path* entry_iterator::operator->() const
{
  return mp_cur_path;
}