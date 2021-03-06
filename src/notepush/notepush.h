/*
 * This file is part of Notepush.
 * 
 * See the COPYRIGHT file at the top-level directory of this distribution
 * for details of code ownership.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef CMAKE_NOTEPUSH_H
#define CMAKE_NOTEPUSH_H

#include <iostream>
#include <git2.h>

namespace notepush {

/**
 * Writes the user's note to a chosen file.
 *
 * The file need not exist, and will be created if required. This function appends to already existing files.
 * 
 * @param path File to be written to .
 * @param note The note to be written.
 */
void write_note(const std::string &path, const std::string &note);

/**
 * Gets a nicely formatted c-string of the current system time.
 *
 * This timestamp accompanies the note in the note files.
 *
 * @return c-string of current time.
 */
char *get_current_time();

/**
 * Gets the HEAD of the repository.
 * 
 * Notepush always targets the most up to date version of the branch, hence the need for this function.
 *
 * @param repo Pointer to repository to operate on.
 * @return git_commit object of the most recent commit
 */
git_commit *get_last_commit(git_repository *repo);
}

#endif //CMAKE_NOTEPUSH_H