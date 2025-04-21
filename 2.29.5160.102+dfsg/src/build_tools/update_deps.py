# -*- coding: utf-8 -*-
# Copyright 2010-2021, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""A helper script to update OSS Mozc build dependencies.

This helper script takes care of updaring build dependencies for legacy GYP
build for OSS Mozc.
"""

import argparse
from collections.abc import Iterator
import dataclasses
import hashlib
import os
import pathlib
import shutil
import subprocess
import sys
import tarfile
import time
import zipfile

import requests


ABS_SCRIPT_PATH = pathlib.Path(__file__).absolute()
# src/build_tools/fetch_deps.py -> src/
ABS_MOZC_SRC_DIR = ABS_SCRIPT_PATH.parents[1]
ABS_THIRD_PARTY_DIR = ABS_MOZC_SRC_DIR.joinpath('third_party')
CACHE_DIR = ABS_MOZC_SRC_DIR.joinpath('third_party_cache')
TIMEOUT = 600


@dataclasses.dataclass
class ArchiveInfo:
  """Third party archive file to be used to build Mozc binaries.

  Attributes:
    dest: Destination directory name under the third_party directory.
    url: URL of the archive.
    size: File size of the archive.
    sha256: SHA-256 of the archive.
  """
  dest: str
  url: str
  size: int
  sha256: str

  @property
  def filename(self) -> str:
    """The filename of the archive."""
    return self.url.split('/')[-1]

  def __hash__(self):
    return hash(self.sha256)


QT = ArchiveInfo(
    dest='qt',
    url='https://download.qt.io/archive/qt/5.15/5.15.9/submodules/qtbase-everywhere-opensource-src-5.15.9.tar.xz',
    size=50389220,
    sha256='1947deb9d98aaf46bf47e6659b3e1444ce6616974470523756c082041d396d1e',
)

JOM = ArchiveInfo(
    dest='qt',
    url='https://download.qt.io/official_releases/jom/jom_1_1_3.zip',
    size=1213852,
    sha256='128fdd846fe24f8594eed37d1d8929a0ea78df563537c0c1b1861a635013fff8',
)

WIX = ArchiveInfo(
    dest='wix',
    url='https://wixtoolset.org/downloads/v3.14.0.6526/wix314-binaries.zip',
    size=41223699,
    sha256='4c89898df3bcab13e12f7ca54399c35ad273475ad2cb6284611d00ae2d063c2c',
)


def get_sha256(path: pathlib.Path) -> str:
  """Returns SHA-256 hash digest of the specified file.

  Args:
    path: Local path the file to calculate SHA-256 about.
  Returns:
    SHA-256 hash digestd of the specified file.
  """
  with open(path, 'rb') as f:
    try:
      # hashlib.file_digest is available in Python 3.11+
      return hashlib.file_digest(f, 'sha256').hexdigest()
    except AttributeError:
      # Fallback to f.read().
      h = hashlib.sha256()
      h.update(f.read())
      return h.hexdigest()


def download(archive: ArchiveInfo, dryrun: bool = False) -> None:
  """Download the specified file.

  Args:
    archive: ArchiveInfo to be downloaded.
    dryrun: True if this is a dry-run.

  Raises:
    RuntimeError: When the downloaded file looks to be corrupted.
  """

  path = CACHE_DIR.joinpath(archive.filename)
  if path.exists():
    if (
        path.stat().st_size == archive.size
        and get_sha256(path) == archive.sha256
    ):
      # Cache hit.
      return
    else:
      if dryrun:
        print(f'dryrun: Verification failed. removing {path}')
      else:
        path.unlink()

  if dryrun:
    print(f'Download {archive.url} to {path}')
    return

  CACHE_DIR.mkdir(parents=True, exist_ok=True)
  saved = 0
  hasher = hashlib.sha256()
  with requests.get(archive.url, stream=True, timeout=TIMEOUT) as r:
    with ProgressPrinter() as printer:
      with open(path, 'wb') as f:
        for chunk in r.iter_content(chunk_size=8192):
          f.write(chunk)
          hasher.update(chunk)
          saved += len(chunk)
          printer.print_line(f'{archive.filename}: {saved}/{archive.size}')
  if saved != archive.size:
    raise RuntimeError(
        f'{archive.filename} size mismatch.'
        f' expected={archive.size} actual={saved}'
    )
  actual_sha256 = hasher.hexdigest()
  if actual_sha256 != archive.sha256:
    raise RuntimeError(
        f'{archive.filename} sha256 mismatch.'
        f' expected={archive.sha256} actual={actual_sha256}'
    )


class ProgressPrinter:
  """A utility to print progress message with carriage return and trancatoin."""

  def __enter__(self):
    if not sys.stdout.isatty():

      class NoOpImpl:
        """A no-op implementation in case stdout is not attached to concole."""

        def print_line(self, msg: str) -> None:
          """No-op implementation.

          Args:
            msg: Unused.
          """
          del msg  # Unused
          return

      self.cleaner = None
      return NoOpImpl()

    class Impl:
      """A real implementation in case stdout is attached to concole."""
      last_output_time_ns = time.time_ns()

      def print_line(self, msg: str) -> None:
        """Print the given message with carriage return and trancatoin.

        Args:
          msg: Message to be printed.
        """
        colmuns = os.get_terminal_size().columns
        now = time.time_ns()
        if (now - self.last_output_time_ns) < 25000000:
          return
        msg = msg + ' ' * max(colmuns - len(msg), 0)
        msg = msg[0 : (colmuns)] + '\r'
        sys.stdout.write(msg)
        sys.stdout.flush()
        self.last_output_time_ns = now

    class Cleaner:
      def cleanup(self) -> None:
        colmuns = os.get_terminal_size().columns
        sys.stdout.write(' ' * colmuns + '\r')
        sys.stdout.flush()

    self.cleaner = Cleaner()
    return Impl()

  def __exit__(self, *exc):
    if self.cleaner:
      self.cleaner.cleanup()


def qt_extract_filter(
    members: Iterator[tarfile.TarInfo],
) -> Iterator[tarfile.TarInfo]:
  """Custom extract filter for the Qt Tar file.

  This custom filter can be used to adjust directory structure and drop
  unnecessary files/directories to save disk space.

  Args:
    members: an iterator of TarInfo from the Tar file.

  Yields:
    An iterator of TarInfo to be extracted.
  """
  with ProgressPrinter() as printer:
    for info in members:
      paths = info.name.split('/')
      if '..' in paths:
        continue
      if len(paths) < 1:
        continue
      paths = paths[1:]
      new_path = '/'.join(paths)
      if len(paths) >= 1 and paths[0] == 'examples':
        printer.print_line('skipping   ' + new_path)
        continue
      else:
        printer.print_line('extracting ' + new_path)
        info.name = new_path
        yield info


def wix_extract_filter(
    members: Iterator[zipfile.ZipInfo],
) -> Iterator[zipfile.ZipInfo]:
  """Custom extract filter for the WiX Zip archive.

  This custom filter can be used to adjust directory structure and drop
  unnecessary files/directories to save disk space.

  Args:
    members: an iterator of ZipInfo from the Zip archive.

  Yields:
    an iterator of ZipInfo to be extracted.
  """
  with ProgressPrinter() as printer:
    for info in members:
      paths = info.filename.split('/')
      if '..' in paths:
        continue
      if len(paths) >= 2:
        printer.print_line('skipping   ' + info.filename)
        continue
      else:
        printer.print_line('extracting ' + info.filename)
        yield info


def extract(
    archive: ArchiveInfo,
    dryrun: bool = False,
) -> None:
  """Extract the given archive.

  Args:
    archive: ArchiveInfo to be exptracted.
    dryrun: True if this is a dry-run.
  """
  dest = ABS_THIRD_PARTY_DIR.joinpath(archive.dest).absolute()
  src = CACHE_DIR.joinpath(archive.filename)
  if src.suffix == '.xz':
    if dryrun:
      print(f'dryrun: Extracting {src}')
    else:
      with tarfile.open(src, mode='r|xz') as f:
        if archive == QT:
          f.extractall(path=dest, members=qt_extract_filter(f))
        else:
          f.extractall(path=dest)
  elif src.suffix == '.zip':
    if dryrun:
      print(f'dryrun: Extracting {src}')
    else:
      def filename(members: Iterator[zipfile.ZipInfo]):
        for info in members:
          yield info.filename
      with zipfile.ZipFile(src) as z:
        if archive == WIX:
          z.extractall(
              path=dest, members=filename(wix_extract_filter(z.infolist()))
          )
        else:
          z.extractall(path=dest)


def is_windows() -> bool:
  """Returns true if the platform is Windows."""
  return os.name == 'nt'


def is_mac() -> bool:
  """Returns true if the platform is Mac."""
  return os.name == 'posix' and os.uname()[0] == 'Darwin'


def update_submodules(dryrun: bool = False) -> None:
  """Run 'git submodule update --init --recursive'.

  Args:
    dryrun: true to perform dryrun.
  """
  command = ' '.join(['git', 'submodule', 'update', '--init', '--recursive'])
  if dryrun:
    print(f'dryrun: subprocess.run({command}, shell=True, check=True)')
  else:
    subprocess.run(command, shell=True, check=True)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--dryrun', action='store_true', default=False)
  parser.add_argument('--noqt', action='store_true', default=False)
  parser.add_argument('--nowix', action='store_true', default=False)
  parser.add_argument('--nosubmodules', action='store_true', default=False)
  parser.add_argument('--cache_only', action='store_true', default=False)

  args = parser.parse_args()

  archives = []
  if (not args.noqt) and (is_windows() or is_mac()):
    archives.append(QT)
    if is_windows():
      archives.append(JOM)
  if (not args.nowix) and is_windows():
    archives.append(WIX)

  for archive in archives:
    download(archive, args.dryrun)

  if args.cache_only:
    return

  dest_dirs = set()
  for archive in archives:
    dest_dirs.add(ABS_THIRD_PARTY_DIR.joinpath(archive.dest))

  for dest_dir in dest_dirs:
    if dest_dir.exists():
      if args.dryrun:
        print(f"dryrun: shutil.rmtree(r'{dest_dir}')")
      else:
        shutil.rmtree(dest_dir)

  for archive in archives:
    extract(archive, args.dryrun)

  if not args.nosubmodules:
    update_submodules(args.dryrun)


if __name__ == '__main__':
  main()
