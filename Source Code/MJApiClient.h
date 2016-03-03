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

#import "MJApiRequest.h"
#import "MJApiUploadRequest.h"
#import "MJApiResponse.h"
#import "MJApiRequestExecutor.h"

/**
 * Cache managmenet flags.
 **/
typedef NS_ENUM(NSInteger, MJApiClientCacheManagement)
{
    /** Default cache management. */
    MJApiClientCacheManagementDefault,
    
    /** When offline (no reachability to the internet), cache will be used. */
    MJApiClientCacheManagementOffline
};

/**
 * Debug logs flags.
 **/
typedef NS_OPTIONS(NSUInteger, MJApiClientLogLevel)
{
    /** No logs will be done. */
    MJApiClientLogLevelNone         = 0,
    
    /** Requests will be logged (including a curl). */
    MJApiClientLogLevelRequests     = 1 << 0,
    
    /** Responses will be logged. */
    MJApiClientLogLevelResponses    = 1 << 1,
};

typedef NS_OPTIONS(NSUInteger, MJApiClientRequestSerializerType)
{
    /** applicaiton/JSON */
    MJApiClientRequestSerializerTypeJSON = 0,

    /** applicaiton/x-www-form-urlencoded with utf8 charset */
    MJApiClientRequestSerializerTypeFormUrlencoded = 1,
};

typedef NS_OPTIONS(NSUInteger, MJApiClientResponseSerializerType)
{
    /** JSON responses */
    MJApiClientResponseSerializerTypeJSON = 0,
    
    /** RAW responses */
    MJApiClientResponseSerializerTypeRaw = 1,
};

@protocol MJApiClientDelegate;

/* ************************************************************************************************** */
#pragma mark -

/**
 * The configurator object.
 **/
@interface MJAPiClientConfigurator : NSObject

/** ************************************************* **
 * @name Configurable properties
 ** ************************************************* **/

/**
 * The host of the API client. Default value is nil.
 **/
@property (nonatomic, strong, readwrite) NSString *host;

/**
 * An aditional API route path to be inserted after the host and before the REST arguments. Default value is nil.
 * @discussion Must be prefixed with "/"
 **/
@property (nonatomic, strong, readwrite) NSString *apiPath;

/**
 * The cache managemenet strategy. Default value is `MJApiClientCacheManagementDefault`.
 **/
@property (nonatomic, assign, readwrite) MJApiClientCacheManagement cacheManagement;

/**
 * The request serializer type. Default value is `MJApiClientRequestSerializerTypeJSON`.
 **/
@property (nonatomic, assign, readwrite) MJApiClientRequestSerializerType requestSerializerType;

/**
 * The response serializer type. Default value is `MJApiClientResponseSerializerTypeJSON`.
 **/
@property (nonatomic, assign, readwrite) MJApiClientResponseSerializerType responseSerializerType;

/**
 * Requests completion block will be executed on the given queue.
 * @discussion If nil, blocks will be executed on the main queue.
 **/
@property (nonatomic, strong, readwrite) dispatch_queue_t completionBlockQueue;

@end

/* ************************************************************************************************** */
#pragma mark -

/**
 * An API client.
 * This class build on top of AFNetworking a user-friendly interface to manage API requests and responses. 
 * The only constraint is that the backend services must always accept and return application/json HTTP requests and responses.
 **/
@interface MJApiClient : NSObject <MJApiRequestExecutor>

/** ************************************************* **
 * @name Getting the default manager
 ** ************************************************* **/

/**
 * Deprecated initializer.
 * @param host The host.
 * @param apiPath Additiona API route
 * @return An initialized instance.
 **/
- (id)initWithHost:(NSString*)host apiPath:(NSString *)apiPath;

/**
 *  Designated initializer.
 *  @param configuratorBlock A MJApiClientConfigurator block
 *  @return The instance initialized
 */
- (id)initWithConfigurator:(void (^)(MJAPiClientConfigurator *configurator))configuratorBlock;

/** ************************************************* **
 * @name Configuring the client
 ** ************************************************* **/

/**
 * The host of the API client.
 **/
@property (nonatomic, strong, readonly) NSString *host;

/**
 * An aditional API route path to be inserted after the host and before the REST arguments.
 * @discussion Must be prefixed with "/"
 **/
@property (nonatomic, strong, readonly) NSString *apiPath;

/**
 * The cache managemenet strategy.
 **/
@property (nonatomic, assign, readonly) MJApiClientCacheManagement cacheManagement;

/**
 * Requests completion block will be executed on the given queue.
 * @discussion If nil, blocks will be executed on the main queue.
 **/
@property (nonatomic, strong, readonly) dispatch_queue_t completionBlockQueue;

/** ************************************************* **
 * @name Authorization Headers
 ** ************************************************* **/

/**
 * Set a barear token (typically from OAuth access tokens). Replaces the basic authentication header.
 * @param The authorization token.
 * @discussion If nil, this method will remove the bearer token header.
 **/
- (void)setBearerToken:(NSString*)token;

/**
 * Set a basic authorization setup. Replaces the bearer token header.
 * @param username The username.
 * @param password The password.
 * @discussion If username or password are nil (or both are nil), this method will remove the basic authentication header.
 **/
- (void)setBasicAuthWithUsername:(NSString*)username password:(NSString*)password;

/**
 * Clears all authorization headers.
 **/
- (void)removeAuthorizationHeaders;

/** ************************************************* **
 * @name Delegate
 ** ************************************************* **/

/**
 * Delegate object.
 **/
@property (nonatomic, weak) id <MJApiClientDelegate> delegate;

/** ************************************************* **
 * @name Logging
 ** ************************************************* **/

/**
 * The log level of the api client. Default value is `MJApiClientLogLevelNone`.
 **/
@property (nonatomic, assign) MJApiClientLogLevel logLevel;

@end

/* ************************************************************************************************** */
#pragma mark -

/**
 * Delegate of an API client.
 **/
@protocol MJApiClientDelegate <NSObject>

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
- (NSError*)apiClient:(MJApiClient*)apiClient errorForResponseBody:(id)responseBody httpResponse:(NSHTTPURLResponse*)httpResponse incomingError:(NSError*)error;

/**
 * Notifies the delegate that an api response has got an error.
 * @param apiClient The API client.
 * @param response The API response with an error.
 **/
- (void)apiClient:(MJApiClient*)apiClient didReceiveErrorInResponse:(MJApiResponse*)response;

@end
