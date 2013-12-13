/*
 * $Id: kvdb.i $
 *
 * Author: Markus Stenberg <fingon@iki.fi>
 *
 * Copyright (c) 2013 Markus Stenberg
 *
 * Created:       Wed Jul 24 13:13:17 2013 mstenber
 * Last modified: Wed Jul 24 13:14:26 2013 mstenber
 * Edit time:     0 min
 *
 */

%module kvdbc
%include "typemaps.i"

%{
  #include "kvdb.h"
  %}

// XXX - typemapping for supported languages

%include "kvdb.h"
