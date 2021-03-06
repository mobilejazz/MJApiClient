//
// Copyright 2015 Mobile Jazz SL
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

#import "HMOAuthSession.h"

#import "HMClientKeychainManager.h"
#import "NSString+HMClientMD5Hashing.h"

@implementation HMOAuthSesionConfigurator

@end

#pragma mark -

@interface HMOAuthSession ()

@property (nonatomic, strong, readwrite) HMOAuth *oauthForAppAccess;
@property (nonatomic, strong, readwrite) HMOAuth *oauthForUserAccess;

@end

@implementation HMOAuthSession
{
    NSOperationQueue *_requestOperationQueue;
    
    HMClient *_apiClient;
    NSString *_apiOAuthPath;
    NSString *_clientId;
    NSString *_clientSecret;
    BOOL _useAppToken;
    
    NSTimeInterval _validTokenOffsetTimeInterval;
    
    HMOAuthConfiguration *_oauthConfiguration;
    
    NSString *_identifier;
}

+ (void)initialize
{
    // Adding old implementation of MJApiOAuth to maintain compatibility with old versions of Hermod.
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        [NSKeyedUnarchiver setClass:HMOAuth.class forClassName:@"MJApiOAuth"];
    });
}

- (id)init
{
    return [self initWithConfigurator:^(HMOAuthSesionConfigurator *configurator) {
        // Nothing to do
    }];
}

- (id)initWithConfigurator:(void (^)(HMOAuthSesionConfigurator *configurator))configuratorBlock;
{
    self = [super init];
    if (self)
    {
        // One request at a time.
        _requestOperationQueue = [[NSOperationQueue alloc] init];
        _requestOperationQueue.maxConcurrentOperationCount = 1;
        
        HMOAuthSesionConfigurator *configurator = [[HMOAuthSesionConfigurator alloc] init];
        configurator.validTokenOffsetTimeInterval = 60;
        configurator.useAppToken = YES;
        configurator.oauthConfiguration = [[HMOAuthConfiguration alloc] init];
        
        if (configuratorBlock)
            configuratorBlock(configurator);
        
        _apiClient = configurator.apiClient;
        _apiOAuthPath = configurator.apiOAuthPath;
        _clientId = configurator.clientId;
        _clientSecret = configurator.clientSecret;
        _oauthConfiguration = configurator.oauthConfiguration;
        _validTokenOffsetTimeInterval = configurator.validTokenOffsetTimeInterval;
        _useAppToken = configurator.useAppToken;
        
        NSString *string = [NSString stringWithFormat:@"host:%@::clientId:%@", _apiClient.serverPath, _clientId];
        _identifier = [string mjz_api_md5_stringWithMD5Hash];

        [self mjz_load];
    }
    return self;
}

#pragma mark Properties

- (void)setOauthForAppAccess:(HMOAuth *)oauthForAppAccess
{
    _oauthForAppAccess = oauthForAppAccess;
    [self mjz_refreshApiClientAuthorization];
    [self mjz_save];
    
    if ([_delegate respondsToSelector:@selector(session:didConfigureOAuth:forSessionAccess:)])
        [_delegate session:self didConfigureOAuth:oauthForAppAccess forSessionAccess:HMOAuthSesionAccessApp];
}

- (void)setOauthForUserAccess:(HMOAuth *)oauthForUserAccess
{
    _oauthForUserAccess = oauthForUserAccess;
    [self mjz_refreshApiClientAuthorization];
    [self mjz_save];
    
    if ([_delegate respondsToSelector:@selector(session:didConfigureOAuth:forSessionAccess:)])
        [_delegate session:self didConfigureOAuth:oauthForUserAccess forSessionAccess:HMOAuthSesionAccessUser];
}

#pragma mark Public Methods

- (void)validateOAuth:(void (^)(void))block
{
    NSBlockOperation *operation = [NSBlockOperation blockOperationWithBlock:^{
        
        // This operation must be alive until the given block can be called safely
        // having a valid token or an error while refreshnig the token happens.
        
        dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
        
        void (^endOperation)(BOOL succeed) = ^(BOOL succeed){
            dispatch_semaphore_signal(semaphore);
            
            if ([NSThread isMainThread])
            {
                if (block)
                    block();
            }
            else
            {
                dispatch_async(dispatch_get_main_queue(), ^{
                    if (block)
                        block();
                });
            }
        };
        
        if (_oauthForUserAccess != nil)
        {
            // Checking validity of the user oauth token.
            if ([_oauthForUserAccess isValidWithOffset:_validTokenOffsetTimeInterval])
            {
                endOperation(YES);
            }
            else
            {
                // Validating app token
                [self mjz_appTokenBlock:^(BOOL succeed) {
                                        
                    // Refreshing user token
                    [self mjz_refreshToken:_oauthForUserAccess.refreshToken completionBlock:^(HMOAuth *oauth, NSError *error) {
                        if (!error)
                            self.oauthForUserAccess = oauth;
                        else
                            self.oauthForUserAccess = nil;
                        
                        endOperation(error == nil);
                    }];
                }];
            }
        }
        else 
        {
            // Checking validity of the app oauth token.
            [self mjz_appTokenBlock:^(BOOL succeed) {
                endOperation(succeed);
            }];
        }
        
        dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
        semaphore = NULL;
    }];
    
    [_requestOperationQueue addOperation:operation];
}

- (void)logout
{
    _oauthForAppAccess = nil;
    _oauthForUserAccess = nil;
    
    [self mjz_refreshApiClientAuthorization];
    [self mjz_save];
    
    if ([_delegate respondsToSelector:@selector(session:didConfigureOAuth:forSessionAccess:)])
        [_delegate session:self didConfigureOAuth:nil forSessionAccess:HMOAuthSesionAccessApp];
    
    if ([_delegate respondsToSelector:@selector(session:didConfigureOAuth:forSessionAccess:)])
        [_delegate session:self didConfigureOAuth:nil forSessionAccess:HMOAuthSesionAccessUser];
}

- (void)loginWithUsername:(NSString*)username password:(NSString*)password completionBlock:(void (^)(NSError *error))completionBlock
{
    NSAssert(username != nil, @"username cannot be nil.");
    NSAssert(password != nil, @"password cannot be nil.");
    NSAssert(_apiOAuthPath != nil, @"API OAuth path is not set.");
    NSAssert(_clientId != nil, @"client id is not set.");
    NSAssert(_clientSecret != nil, @"client secret is not set.");
    
    HMRequest *request = [HMRequest requestWithPath:_apiOAuthPath];
    request.httpMethod = HMHTTPMethodPOST;
    request.parameters = @{@"username": username,
                           @"password": password,
                           @"grant_type": @"password",
                           @"client_id": _clientId,
                           @"client_secret": _clientSecret,
                           };
    
    [self validateOAuth:^{
        [_apiClient performRequest:request apiPath:nil completionBlock:^(HMResponse *response) {
            if (response.error == nil)
            {
                HMOAuth *oauth = [[HMOAuth alloc] initWithJSON:response.responseObject configuration:_oauthConfiguration];
                self.oauthForUserAccess = oauth;

                if (completionBlock)
                    completionBlock(nil);
            }
            else
            {
                if (completionBlock)
                    completionBlock(response.error);
            }
        }];
    }];
}

- (void)configureWithOAuth:(HMOAuth*)oauth forSessionAccess:(HMOAuthSesionAccess)sessionAccess
{
    if (sessionAccess == HMOAuthSesionAccessUser)
    {
        _oauthForUserAccess = oauth;
    }
    else if (sessionAccess == HMOAuthSesionAccessApp)
    {
        _oauthForAppAccess = oauth;
    }
    
    [self mjz_refreshApiClientAuthorization];
    [self mjz_save];
}

#pragma mark Private Mehtods

- (void)mjz_appTokenBlock:(void (^)(BOOL succeed))block
{
    if (_useAppToken == NO)
    {
        if (block)
            block(YES);
        return;
    }
    
    // Checking validity of the app oauth token.
    if ([_oauthForAppAccess isValidWithOffset:_validTokenOffsetTimeInterval])
    {
        if (block)
            block(YES);
    }
    else
    {
        [self mjz_clientCredentialsWithCompletionBlock:^(HMOAuth *oauth, NSError *error) {
            if (!error)
                self.oauthForAppAccess = oauth;
            else
                self.oauthForAppAccess = nil;
            
            if (block)
                block(error == nil);
        }];
    }
}

- (NSString*)mjz_keychainOAuthAppKey
{
    NSString *key = [NSString stringWithFormat:@"%@.oauth.app", _identifier];
    return key;
}

- (NSString*)mjz_keychainOAuthUserKey
{
    NSString *key = [NSString stringWithFormat:@"%@.oauth.user", _identifier];
    return key;
}

- (void)mjz_refreshToken:(NSString*)refreshToken completionBlock:(void (^)(HMOAuth *oauth, NSError *error))completionBlock
{
    NSAssert(_apiOAuthPath != nil, @"API OAuth path is not set.");
    NSAssert(_clientId != nil, @"client id is not set.");
    NSAssert(_clientSecret != nil, @"client secret is not set.");
    
    if (!refreshToken)
    {
        // If refresh token is not available, returning an HTTP Bad Request (400) error.
        if (completionBlock)
            completionBlock(nil, [NSError errorWithDomain:NSURLErrorDomain
                                                     code:400
                                                 userInfo:@{NSLocalizedDescriptionKey: @"Cannot refresh token because no refresh_token is provided"}]);
        return;
    }
    
    HMRequest *request = [HMRequest requestWithPath:_apiOAuthPath];
    request.httpMethod = HMHTTPMethodPOST;
    request.parameters = @{@"grant_type": @"refresh_token",
                           @"refresh_token": refreshToken,
                           @"client_id": _clientId,
                           @"client_secret": _clientSecret,
                           };
    
    [_apiClient performRequest:request apiPath:nil completionBlock:^(HMResponse *response) {
        if (response.error == nil)
        {
            HMOAuth *oauth = [[HMOAuth alloc] initWithJSON:response.responseObject configuration:_oauthConfiguration];
            
            if (completionBlock)
                completionBlock(oauth, nil);
        }
        else
        {
            if (completionBlock)
                completionBlock(nil, response.error);
        }
    }];
}

- (void)mjz_clientCredentialsWithCompletionBlock:(void (^)(HMOAuth *oauth, NSError *error))completionBlock
{
    NSAssert(_apiOAuthPath != nil, @"API OAuth path is not set.");
    NSAssert(_clientId != nil, @"client id is not set.");
    NSAssert(_clientSecret != nil, @"client secret is not set.");
    
    HMRequest *request = [HMRequest requestWithPath:_apiOAuthPath];
    request.httpMethod = HMHTTPMethodPOST;
    request.parameters = @{@"grant_type": @"client_credentials",
                           @"client_id": _clientId,
                           @"client_secret": _clientSecret,
                           };
    
    [_apiClient performRequest:request apiPath:nil completionBlock:^(HMResponse *response) {
        if (response.error == nil)
        {
            HMOAuth *oauth = [[HMOAuth alloc] initWithJSON:response.responseObject configuration:_oauthConfiguration];
            
            if (completionBlock)
                completionBlock(oauth, nil);
        }
        else
        {
            if (completionBlock)
                completionBlock(nil, response.error);
        }
    }];
}

- (void)mjz_load
{
    NSData *appData = [[self mjz_keychainManager] keychainDataForKey:[self mjz_keychainOAuthAppKey]];
    NSData *userData = [[self mjz_keychainManager] keychainDataForKey:[self mjz_keychainOAuthUserKey]];
    
    if (appData)
    {
        HMOAuth *appOauth = [NSKeyedUnarchiver unarchiveObjectWithData:appData];
        if (appOauth.accessToken && appOauth.refreshToken)
        {
            _oauthForAppAccess = appOauth;
        }
    }
    
    if (userData) {
        HMOAuth *userOauth = [NSKeyedUnarchiver unarchiveObjectWithData:userData];
        if (userOauth.accessToken && userOauth.refreshToken)
        {
            _oauthForUserAccess = userOauth;
        }
    }
    
    [self mjz_refreshApiClientAuthorization];
}

- (void)mjz_save
{
    if (_oauthForAppAccess)
    {
        NSData *data = [NSKeyedArchiver archivedDataWithRootObject:_oauthForAppAccess];
        [[self mjz_keychainManager] setKeychainData:data forKey:[self mjz_keychainOAuthAppKey]];
    }
    else
    {
        [[self mjz_keychainManager] removeKeychainEntryForKey:[self mjz_keychainOAuthAppKey]];
    }
    
    if (_oauthForUserAccess)
    {
        NSData *data = [NSKeyedArchiver archivedDataWithRootObject:_oauthForUserAccess];
        [[self mjz_keychainManager] setKeychainData:data forKey:[self mjz_keychainOAuthUserKey]];
    }
    else
    {
        [[self mjz_keychainManager] removeKeychainEntryForKey:[self mjz_keychainOAuthUserKey]];
    }
}

- (void)mjz_refreshApiClientAuthorization
{
    HMOAuthSesionAccess access = HMOAuthSesionAccessNone;
    HMOAuth *oauth = nil;
    
    if (_oauthForUserAccess != nil)
    {
        oauth = _oauthForUserAccess;
        access = HMOAuthSesionAccessUser;
    }
    else if (_oauthForAppAccess != nil)
    {
        oauth = _oauthForAppAccess;
        access = HMOAuthSesionAccessApp;
    }
    
    // Set the oauth authorization headers
    if (oauth)
        [_apiClient setBearerToken:oauth.accessToken];
    else
        [_apiClient removeAuthorizationHeaders];
    
    // update the session access flag
    if (access != _sessionAccess)
    {
        [self willChangeValueForKey:@"sessionAccess"];
        _sessionAccess = access;
        [self didChangeValueForKey:@"sessionAccess"];
    }
}

- (HMClientKeychainManager*)mjz_keychainManager
{
    static NSString *service = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        service = [NSString stringWithFormat:@"%@.%@",[[NSBundle mainBundle] bundleIdentifier], _identifier];
    });
    
    HMClientKeychainManager *manager = [HMClientKeychainManager managerForService:service];
    return manager;
}

#pragma mark - Protocols
#pragma mark HMRequestExecutor

- (void)performRequest:(HMRequest *)request completionBlock:(HMResponseBlock)completionBlock
{
    [self validateOAuth:^{
        [_apiClient performRequest:request completionBlock:completionBlock];
    }];
}

- (void)performRequest:(HMRequest *)request apiPath:(NSString *)apiPath completionBlock:(HMResponseBlock)completionBlock
{
    [self validateOAuth:^{
        [_apiClient performRequest:request completionBlock:completionBlock];
    }];
}

@end
