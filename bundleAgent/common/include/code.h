/*
* Copyright (c) 2014 SeNDA
* 
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
* 
*     http://www.apache.org/licenses/LICENSE-2.0
* 
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
* 
*/

#ifndef INC_COMMON_CRYPTO_H
#define INC_COMMON_CRYPTO_H

int B64_encoded_l(int decoded_lenght);
int B64_decoded_l(char *encoded, int encoded_l);
int B64_encode(unsigned char *in, int inl, unsigned char *out);
int B64_decode(unsigned char *in, int inl, unsigned char *out);
int hex_code(unsigned char *in, int inl, char *out);
int hex_decode(unsigned char *in, int inl, char *out);
unsigned long Crc32_ComputeBuf( unsigned long inCrc32, const void *buf, size_t bufLen );
#endif