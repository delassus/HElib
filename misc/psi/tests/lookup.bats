#!/usr/bin/env bats

# Copyright (C) 2020 IBM Corp.
# This program is Licensed under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance
# with the License. You may obtain a copy of the License at
#   http://www.apache.org/licenses/LICENSE-2.0
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License. See accompanying LICENSE file.

utils_dir="../../../../utils"
load "../../../utils/tests/std"

nslots=3
modulus=13
#nslots=13860
#modulus=257
#nslots=79872
#modulus=1278209
datadir="data_and_params"
data_prefix="../$datadir"
db_encoded="../db${nslots}.encoded"
query_encoded="../query${nslots}.encoded"
lookup="../../build/bin/lookup"

# Run once per test
function setup {
  mkdir -p $tmp_folder
  cd $tmp_folder
  print-info-location
}

# Run once per test
function teardown {
  cd -
  remove-test-directory "$tmp_folder"
}

@test "lookup 64 threads" {
  
  echo "lookup 64 threads" > README
  $lookup ${data_prefix}/${prefix_bgv}.pk $data_prefix/db.ctxt $data_prefix/query.ctxt result.ctxt -n=64

  $decrypt ${data_prefix}/${prefix_bgv}.sk result.ctxt -o "result.ptxt"
  $decrypt ${data_prefix}/${prefix_bgv}.sk result.ctxt_a -o "result.ptxt_a"
  $decrypt ${data_prefix}/${prefix_bgv}.sk result.ctxt_!a -o "result.ptxt_!a"
  $decrypt ${data_prefix}/${prefix_bgv}.sk result.ctxt_b -o "result.ptxt_b"
  $decrypt ${data_prefix}/${prefix_bgv}.sk result.ctxt_c -o "result.ptxt_c"
  $decrypt ${data_prefix}/${prefix_bgv}.sk result.ctxt_and -o "result.ptxt_and"
  $decrypt ${data_prefix}/${prefix_bgv}.sk result.ctxt_or -o "result.ptxt_or"
  $decrypt ${data_prefix}/${prefix_bgv}.sk result.ctxt_expand1 -o "result.ptxt_expand1"
  $decrypt ${data_prefix}/${prefix_bgv}.sk result.ctxt_expand2 -o "result.ptxt_expand2"
  $decrypt ${data_prefix}/${prefix_bgv}.sk result.ctxt_aOr_!bAndc -o "result.ptxt_aOr_!bAndc"
  $decrypt ${data_prefix}/${prefix_bgv}.sk result.ctxt_aAnd!b -o "result.ptxt_aAnd!b"
  

  ../gen-expected-mask.py ${query_encoded} ${db_encoded} --mod-p $modulus --test SAME > "expected.mask"
  ../gen-expected-mask.py ${query_encoded} ${db_encoded} --mod-p $modulus --test AND > "expected.mask_and"
  ../gen-expected-mask.py ${query_encoded} ${db_encoded} --mod-p $modulus --test OR > "expected.mask_or"
  ../gen-expected-mask.py ${query_encoded} ${db_encoded} --mod-p $modulus --test EXPAND > "expected.mask_expand"

  diff "result.ptxt" "expected.mask"
  diff "result.ptxt_and" "expected.mask_and"
  diff "result.ptxt_or" "expected.mask_or"
  diff "result.ptxt_expand" "expected.mask_expand"
}
