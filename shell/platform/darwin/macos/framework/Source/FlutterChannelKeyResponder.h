// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "flutter/shell/platform/darwin/macos/framework/Source/FlutterKeyPrimaryResponder.h"

#import <Cocoa/Cocoa.h>

#import "flutter/shell/platform/darwin/common/framework/Headers/FlutterChannels.h"

/**
 * A primary responder of |FlutterKeyboardManager| that handles events by
 * sending the raw information through the method channel.
 *
 * This class corresponds to the RawKeyboard API in the framework.
 */
@interface FlutterChannelKeyResponder : NSObject <FlutterKeyPrimaryResponder>

/**
 * Create an instance by specifying the method channel to use.
 */
- (nonnull instancetype)initWithChannel:(nonnull FlutterBasicMessageChannel*)channel;

@end
