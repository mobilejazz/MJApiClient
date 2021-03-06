//
// Copyright 2014 Mobile Jazz SL
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#import <Foundation/Foundation.h>

/**
 * Adding MD5 hash generation from strings.
 **/
@interface NSString (HMClientMD5Hashing)

/**
 * Generates the MD5 hash from the current string.
 * @return The MD5 hash string.
 **/
- (NSString*)mjz_api_md5_stringWithMD5Hash;

@end
