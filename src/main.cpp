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

#include <notepush.h>
#include <main.h>
#include <string>
#include <git2.h>
#include <algorithm>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <unordered_set>
#include <chrono>
#include <memory.h>

struct Options
{
    std::string repo;    // repository directory
    bool online = false; // offline mode - when true, connections to the remote will not be performed.
};

char *get_cmd_option(char **begin, char **end, const std::string &option)
{
    char **itr = std::find(begin, end, option);
    if (itr != end && ++itr != end)
    {
        return *itr;
    }
    return nullptr;
}

bool cmd_option_exists(char **begin, char **end, const std::string &option)
{
    return std::find(begin, end, option) != end;
}

int main(int argc, char *argv[])
{
    if (cmd_option_exists(argv, argv + argc, "-h"))
    {
        std::cout << HEADER << USAGE << "\n";
        return 0;
    }

    if (cmd_option_exists(argv, argv + argc, "-r"))
    {
        std::string answer;
        std::cout << "Are you sure you want to reset your configuration? Y or N\n";
        std::cin >> answer;

        if (answer == "Y")
        {
            std::filesystem::remove("/etc/notepush/psh.config");
        }

        return 0;
    }

    /*  On first boot, create a config file interactively with the user */
    if (!std::filesystem::is_directory("/etc/notepush"))
    {
        std::cerr << "Creating new config directory: /etc/notepush\n";
        std::filesystem::create_directory("/etc/notepush");
    }

    // if true, then config file needs to be created
    if (!std::filesystem::is_regular_file("/etc/notepush/psh.config"))
    {
        std::cerr << "Creating new config file: /etc/notepush/psh.config\n";

        std::string repo;
        std::cout << "Enter repo dir location: ";
        std::cin >> repo;
        std::cout << "\n";

        std::ofstream config("/etc/notepush/psh.config");

        config << repo << std::endl;

        config.close();
    }

    /// load the options/config
    Options options;

    options.online = cmd_option_exists(argv, argv + argc, "-o");

    std::ifstream config_file("/etc/notepush/psh.config");
    std::string line;
    while (std::getline(config_file, line))
    {
        options.repo = std::move(line);
    }

    git_libgit2_init();

    int error;
    git_repository *repo = NULL;

    /* Open repository in given directory (or fail if not a repository) */
    error = git_repository_open_ext(&repo, &options.repo[0], GIT_REPOSITORY_OPEN_NO_SEARCH, NULL);
    if (error != 0)
    {
        std::cerr << "Repository: " << options.repo << " is not a valid git repo.\n";
        std::cerr << "Please create a valid repository in the provided repo path.\n";
        std::cerr << giterr_last()->message << std::endl;

        return 1;
    }

    if (cmd_option_exists(argv, argv + argc, "-a"))
    {
        for (const auto &entry : std::filesystem::directory_iterator(options.repo))
        {

            auto filename = entry.path().filename();
            if (filename == "README.md" || filename == ".git")
            {
                continue;
            }
            std::cout << entry.path().filename() << std::endl;
        }

        return 0;
    }

    if (cmd_option_exists(argv, argv + argc, "-l"))
    {
        std::string topic = get_cmd_option(argv, argv + argc, "-l");

        std::string topic_file(options.repo + "/" + topic + ".txt");

        if (std::filesystem::exists(topic_file))
        {
            std::ifstream file(topic_file);

            if (!file)
            {
                std::cerr << "Unable to open topic file: " << topic_file << std::endl;
                return 1;
            }

            std::cout << "Printing note file: " << topic_file << std::endl;

            std::string line;

            while (std::getline(file, line))
            {
                std::cout << line << std::endl;
            }
        }
        else
        {
            std::cerr << "No file associated with the topic: " << topic << std::endl;
            return 1;
        }

        return 0;
    }



    /*
    The user provides a note with '@' used to delimit the topics
    i.e. psh This is a note @Topic1

    The repo will contain the following files:

    general.txt - All of the notes with timestamps
    Topic1.txt - Any notes tagged with Topic1.
    Topic2.txt - Any notes tagged with Topic2.
    TopicN.txt - Any notes tagged with TopicN.
    */
    std::stringstream note;
    std::unordered_set<std::string> topics;

    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] == '@')
        {
            topics.emplace(std::string(argv[i]));
        }
        else if (argv[i][0] == '-' && argv[i][1] == 'o') // ignore -o option
        {
            continue;
        }
        else
        {
            note << argv[i] << " ";
        }
    }

    if (note.rdbuf()->in_avail() == 0)
    {
        std::cerr << "No note was provided\n";

        return 0;
    }

    if (options.online)
    {
        std::string command = "cd ";
        command += options.repo;
        command += "&& git pull origin master";

        system (&command[0]);
    }

    /// all notes get written to general.
    notepush::write_note(options.repo + "/general.txt", note.str());

    /// write notes to their respective files
    for (const auto &topic : topics)
    {
        notepush::write_note(options.repo + "/" + topic.substr(1) + ".txt", note.str());
    }

    git_index *index = NULL;
    git_oid tree_oid;
    git_signature *signature;
    git_tree *tree;
    git_buf buffer;
    git_commit *head = notepush::get_last_commit(repo);
    git_oid commit_oid;

    memset(&buffer, 0, sizeof(git_buf));

    // git add .
    // Get the git index.
    error = git_repository_index(&index, repo);
    if (error != 0)
        std::cerr << giterr_last()->message << std::endl;

    // Add all files to the git index.
    error = git_index_add_all(index, NULL, 0, NULL, NULL);
    if (error != 0)
        std::cerr << giterr_last()->message << std::endl;

    // Write the index to disk.
    error = git_index_write(index);
    if (error != 0)
        std::cerr << giterr_last()->message << std::endl;

    // END git add .

    // git commit -a -m "Commit made via notepush"
    git_index_write_tree(&tree_oid, index);
    git_signature_new(&signature, "Notepush", "notepush@fake.com", std::time(nullptr), 0);
    git_tree_lookup(&tree, repo, &tree_oid);
    git_message_prettify(&buffer, "Commit made via notepush", 0, '#');
    git_commit_create_v(
            &commit_oid,
            repo,
            "HEAD",
            signature,
            signature,
            NULL,
            buffer.ptr,
            tree,
            1,
            head);
    // END git commit -a -m "Commit made via notepush"

    if (options.online)
    {
        std::string command = "cd ";
        command += options.repo;
        command += "&& git push origin master";

        system (&command[0]);
    }

// Free resources used by libgit2.
    git_buf_free(&buffer);
    git_signature_free(signature);
    git_tree_free(tree);
    git_index_free(index);
    git_repository_free(repo);
    git_commit_free(head);

    return 0;
}
