# -*- coding: utf-8 -*-
# Copyright 2010-2014, Google Inc.
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

"""Utility to generate user_pos_data.h."""

__author__ = "hidehiko"

from collections import defaultdict
import logging
import optparse

from build_tools import code_generator_util
from dictionary import pos_util


def OutputUserPosDataHeader(user_pos_data, output):
  """Prints user_pos_data.h to output."""
  # Output kConjugation
  for index, (_, conjugation_list) in enumerate(user_pos_data):
    output.write(
        'static const UserPOS::ConjugationType kConjugation%d[] = {\n' % (
            index))
    for value_suffix, key_suffix, pos_id in conjugation_list:
      output.write('  { %s, %s, %d },\n' % (
          code_generator_util.ToCppStringLiteral(value_suffix),
          code_generator_util.ToCppStringLiteral(key_suffix),
          pos_id))
    output.write('};\n')

  # Output PosToken
  output.write('const UserPOS::POSToken kPOSToken[] = {\n')
  for index, (user_pos, conjunction_list) in enumerate(user_pos_data):
    output.write('  { %s, %d, kConjugation%d },\n' % (
        code_generator_util.ToCppStringLiteral(user_pos),
        len(conjunction_list),
        index))
  # Also output the sentinal.
  output.write('  { NULL, 0, NULL },\n'
               '};\n')


def ParseOptions():
  parser = optparse.OptionParser()
  # Input: id.def, special_pos.def, user_pos.def, cforms.def
  # Output: user_pos_data.h
  parser.add_option('--id_file', dest='id_file', help='Path to id.def.')
  parser.add_option('--special_pos_file', dest='special_pos_file',
                    help='Path to special_pos.def')
  parser.add_option('--cforms_file', dest='cforms_file',
                    help='Path to cforms.def')
  parser.add_option('--user_pos_file', dest='user_pos_file',
                    help='Path to user_pos,def')
  parser.add_option('--output', dest='output',
                    help='Path to output user_pos_data.h')
  return parser.parse_args()[0]


def main():
  options = ParseOptions()
  pos_database = pos_util.PosDataBase()
  pos_database.Parse(options.id_file, options.special_pos_file)
  inflection_map = pos_util.InflectionMap()
  inflection_map.Parse(options.cforms_file)
  user_pos = pos_util.UserPos(pos_database, inflection_map)
  user_pos.Parse(options.user_pos_file)

  with open(options.output, 'w') as stream:
    OutputUserPosDataHeader(user_pos.data, stream)


if __name__ == '__main__':
  main()
