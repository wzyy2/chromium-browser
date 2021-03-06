// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_STYLE_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_STYLE_H_

// Enum defining the different styles for the toolbar. The value of the enum are
// used as array accessor.
typedef NS_ENUM(NSInteger, ToolbarStyle) {
  // Normal (non-incognito) style.
  NORMAL = 0,
  // Incognito style.
  INCOGNITO = 1
};

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_STYLE_H_
