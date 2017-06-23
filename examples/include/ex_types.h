/*
 * XAmbit - Cross boundary data transfer library
 * Copyright (C) 2016-2017 BAE Systems.
 *
 * This file is part of XAmbit.
 *
 * XAmbit is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * XAmbit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with XAmbit.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


/* These are examples only. It is up to the application developer to define the
 * acceptable types that the application will support. Types are not necissarily
 * the same as file types. Both the sender and receiver MUST agree on the type
 * definitions. */
#define XT_TEXT		1
#define XT_MPG_FRAME	2
#define XT_JPEG		3
#define XT_PNG		4
#define XT_PDF		5
#define XT_BIN		6
#define XT_FILE		7
#define XT_AIVDM	8
#define XT_AIVDO	9
