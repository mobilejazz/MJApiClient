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

#import "HMRequest.h"
#import "HMUploadRequest.h"
#import "HMResponse.h"
#import "HMRequestExecutor.h"
#import "HMConfigurationManager.h"

/**
 * Cache managmenet flags.
 **/
typedef NS_ENUM(NSInteger, HMClientCacheManagement)
{
    /** Default cache management. */
    HMClientCacheManagementDefault,

    /** When offline (no reachability to the internet), cache will be used. */
    HMClientCacheManagementOffline
};

/**
 * Debug logs flags.
 **/
typedef NS_OPTIONS(NSUInteger, HMClientLogLevel)
{
    /** No logs will be done. */
    HMClientLogLevelNone         = 0,

    /** Requests will be logged (including a curl). */
    HMClientLogLevelRequests     = 1 << 0,
    
    /** Responses will be logged. */
    HMClientLogLevelResponses    = 1 << 1,
};

typedef NS_OPTIONS(NSUInteger, HMClientRequestSerializerType)
{
    /** applicaiton/JSON */
    HMClientRequestSerializerTypeJSON = 0,

    /** applicaiton/x-www-form-urlencoded with utf8 charset */
    HMClientRequestSerializerTypeFormUrlencoded = 1,
};

typedef NS_OPTIONS(NSUInteger, HMClientResponseSerializerType)
{
    /** JSON responses */
    HMClientResponseSerializerTypeJSON = 0,

    /** RAW responses */
    HMClientResponseSerializerTypeRaw = 1,
};

@protocol HMClientDelegate;

/* ************************************************************************************************** */
#pragma mark -

/**
 * The configurator object.
 **/
@interface HMClientConfigurator : NSObject

/**
 * Automatic API Configuration.
 **/
- (void)configureWithConfiguration:(HMConfiguration * _Nonnull)configuration;

/** ************************************************* **
 * @name Configurable properties
 ** ************************************************* **/

/**
 * The host of the API client. Default value is nil.
 **/
@property (nonatomic, strong, readwrite, nonnull) NSString *serverPath;

/**
 * An aditional API route path to be inserted after the host and before the REST arguments. Default value is nil.
 * @discussion Must be prefixed with "/"
 **/
@property (nonatomic, strong, readwrite, nullable) NSString *apiPath;

/**
 * The cache managemenet strategy. Default value is `HMClientCacheManagementDefault`.
 **/
@property (nonatomic, assign, readwrite) HMClientCacheManagement cacheManagement;

/**
 * The request serializer type. Default value is `HMClientRequestSerializerTypeJSON`.
 **/
@property (nonatomic, assign, readwrite) HMClientRequestSerializerType requestSerializerType;

/**
 * The response serializer type. Default value is `HMClientResponseSerializerTypeJSON`.
 **/
@property (nonatomic, assign, readwrite) HMClientResponseSerializerType responseSerializerType;

/**
 * The request timeout interval. Default value is 60 seconds.
 **/
@property (nonatomic, assign) NSTimeInterval timeoutInterval;

/**
 * Requests completion block will be executed on the given queue.
 * @discussion If nil, blocks will be executed on the main queue.
 **/
@property (nonatomic, strong, readwrite, nullable) dispatch_queue_t completionBlockQueue;

/**
 * The acceptable MIME types for responses. When non-`nil`, responses with a `Content-Type` with MIME types that do not intersect with the set will result in an error during validation.
 */
@property (nonatomic, copy, nullable) NSSet <NSString *> *acceptableContentTypes;

@end

/* ************************************************************************************************** */
#pragma mark -

/**
 * An API client.
 * This class build on top of AFNetworking a user-friendly interface to manage API requests and responses. 
 * The only constraint is that the backend services must always accept and return application/json HTTP requests and responses.
 **/
@interface HMClient : NSObject <HMRequestExecutor>

/** ************************************************* **
 * @name Main initializers
 ** ************************************************* **/

/**
 * Deprecated initializer.
 * @param serverPath The serverPath. For example: https://www.mydomain.com
 * @param apiPath Additiona API route. For example: /api/v2
 * @return An initialized instance.
 **/
- (instancetype _Nonnull)initWithServerPath:(NSString * _Nonnull)serverPath apiPath:(NSString * _Nullable)apiPath;

/**
 *  Designated initializer.
 *  @param configuratorBlock A HMClientConfigurator block
 *  @return The instance initialized
 */
- (instancetype _Nonnull)initWithConfigurator:(void (^_Nonnull)(HMClientConfigurator * _Nonnull configurator))configuratorBlock;

/** ************************************************* **
 * @name Configuring the client
 ** ************************************************* **/

/**
 * Reconfigure the API client.
 * @param configuratorBlock A HMClientConfigurator block
 **/
- (void)reconfigureWithConfigurator:(void (^_Nonnull)(HMClientConfigurator * _Nonnull configurator))configuratorBlock;

/**
 * The server path of the API client.
 **/
@property (nonatomic, strong, readonly, nonnull) NSString *serverPath;

/**
 * An aditional API route path to be inserted after the host and before the REST arguments.
 * @discussion Must be prefixed with "/"
 **/
@property (nonatomic, strong, readonly, nullable) NSString *apiPath;

/**
 * The cache managemenet strategy.
 **/
@property (nonatomic, assign, readonly) HMClientCacheManagement cacheManagement;

/**
 * Requests completion block will be executed on the given queue.
 * @discussion If nil, blocks will be executed on the main queue.
 **/
@property (nonatomic, strong, readonly, nullable) dispatch_queue_t completionBlockQueue;

/** ************************************************* **
 * @name Authorization Headers
 ** ************************************************* **/

/**
 * Sets a barear token (typically from OAuth access tokens). Replaces the basic authentication header.
 * @param token The authorization token.
 * @discussion If nil, this method will remove the bearer token header.
 **/
- (void)setBearerToken:(NSString * _Nullable)token;

/**
 * Sets a basic authorization setup. Replaces the bearer token header.
 * @param username The username.
 * @param password The password.
 * @discussion If username or password are nil (or both are nil), this method will remove the basic authentication header.
 **/
- (void)setBasicAuthWithUsername:(NSString * _Nonnull)username password:(NSString * _Nonnull)password;

/**
 * Sets a generic "Authorization" value in the Header of the HTTP request.
 * @param value The authorization value.
 * @discussion If nil, this method will remove the authorization header.
 **/
- (void)setAuthorizationHeader:(NSString * _Nullable)value;

/**
 * Clears all authorization headers.
 **/
- (void)removeAuthorizationHeaders;

/** ************************************************* **
 * @name HTTP Headers
 ** ************************************************* **/

/**
 * Dictionary containing additional HTTP header parameters. Default is nil.
 **/
@property (nonatomic, strong, nullable) NSDictionary *headerParameters;

/** ************************************************* **
 * @name Localization
 ** ************************************************* **/

/**
 * If YES, automatically configures the Accept-Language HTTP header to the current device language. Default value is YES.
 **/
@property (nonatomic, assign) BOOL insertAcceptLanguageHeader;

/**
 * If YES, it will insert a language parameter inside all body requests. Default value is NO.
 **/
@property (nonatomic, assign) BOOL insertLanguageAsParameter;

/**
 * The name of the body request language parameter. Default value is "language".
 **/
@property (nonatomic, strong, nonnull) NSString *languageParameterName;

/** ************************************************* **
 * @name Requests
 ** ************************************************* **/

/**
 * A dictionary of parameters that are going to be added to all requests. Default is nil.
 * @discussion Global parameters are added before sending the URL request. If duplicated parameter keys, the values in this dictionary will be the final ones.
 **/
@property (nonatomic, strong, nullable) NSDictionary *requestGlobalParameters;

/** ************************************************* **
 * @name Delegate
 ** ************************************************* **/

/**
 * Delegate object.
 **/
@property (nonatomic, weak, nullable) id <HMClientDelegate> delegate;

/** ************************************************* **
 * @name Logging
 ** ************************************************* **/

/**
 * The log level of the api client. Default value is `HMClientLogLevelNone`.
 **/
@property (nonatomic, assign) HMClientLogLevel logLevel;

@end

/* ************************************************************************************************** */
#pragma mark -

/**
 * Delegate of an API client.
 **/
@protocol HMClientDelegate <NSObject>

/** ************************************************* **
 * @name Managing errors
 ** ************************************************* **/

@optional
/**
 * By implementing this method, the delegate thas the oportunity to create custom errors depending on the incoming response body or the incoming error.
 * @param apiClient The API client.
 * @param responseBody The response body (can be nil).
 * @param httpResponse The HTTP URL Response.
 * @param error The incoming error (can be nil).
 * @discussion This method is called for every succeed and failed API response. Either the response body is not nil, the error is not nil or both are not nil.
 **/
- (NSError * _Nullable)apiClient:(HMClient * _Nonnull)apiClient errorForResponseBody:(id _Nullable)responseBody httpResponse:(NSHTTPURLResponse * _Nonnull)httpResponse incomingError:(NSError * _Nullable)error;

/**
 * Notifies the delegate that an api response has got an error.
 * @param apiClient The API client.
 * @param response The API response with an error.
 **/
- (void)apiClient:(HMClient * _Nonnull)apiClient didReceiveErrorInResponse:(HMResponse * _Nonnull)response;

@end
