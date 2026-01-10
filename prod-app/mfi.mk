# © 2024 Unit Circle Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

$(eval $(call new-artifact))

CFLAGS += -DBUILD_TYPE='"MFI"'
ASMFLAGS += -DBUILD_TYPE='"MFI"'

LDFILE_FLAGS +=  -DLOGDATA_BASE=0x00300000 -DLOGDATA_LEN=0x00010000

include prod-app/app.mki

$(eval $(call executable))
