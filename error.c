/*
 * ISO 13818 stream multiplexer
 * Copyright (C) 2001 Convergence Integrated Media GmbH Berlin
 * Author: Oskar Schirmer (schirmer@scara.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * Module:  Error
 * Purpose: Error and debug information provision.
 *          Other functions related to stderr.
 */

#include "error.h"

char *warn_level_name[] = {
  "Error",
  "Warning",
  "Important",
  "Information",
  "Secondary",
  "Debug"
};

char *warn_module_name[] = {
  "Init",
  "Dispatch",
  "Error",
  "Input",
  "Output",
  "Command",
  "Global",
  "Split PES",
  "Split PS",
  "Split TS",
  "Splice PS",
  "Splice TS",
  "Splice",
  "Descr"
};

int verbose_level;

void do_warn (int level1,
    char *text,
    int module1,
    int func,
    int numb,
    long value)
{
  fprintf (stderr, "%s: %s (%s,%02X,%02X,%08lX/%ld)\n",
    warn_level_name[level1],
    text, warn_module_name[module1], func, numb, value, value);
}

