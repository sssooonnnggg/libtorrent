/*

Copyright (c) 2012, Arvid Norberg
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the distribution.
    * Neither the name of the author nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

*/

#include "libtorrent/file.hpp"
#include "test.hpp"
#include <cstring> // for std::strcmp
#include <vector>
#include <set>
#include <thread>

namespace lt = libtorrent;
using namespace libtorrent;

int touch_file(std::string const& filename, int size)
{
	using namespace libtorrent;

	std::vector<char> v;
	v.resize(size);
	for (int i = 0; i < size; ++i)
		v[i] = i & 255;

	file f;
	error_code ec;
	if (!f.open(filename, file::write_only, ec)) return -1;
	if (ec) return -1;
	file::iovec_t b = {&v[0], v.size()};
	std::int64_t written = f.writev(0, &b, 1, ec);
	if (written != int(v.size())) return -3;
	if (ec) return -3;
	return 0;
}

TORRENT_TEST(create_directory)
{
	error_code ec;
	create_directory("__foobar__", ec);
	TEST_CHECK(!ec);

	file_status st;
	stat_file("__foobar__", &st, ec);
	TEST_CHECK(!ec);

	TEST_CHECK(st.mode & file_status::directory);

	remove("__foobar__", ec);
	TEST_CHECK(!ec);
}

TORRENT_TEST(file_status)
{
	error_code ec;

	// test that the modification timestamps
	touch_file("__test_timestamp__", 10);

	file_status st1;
	stat_file("__test_timestamp__", &st1, ec);
	TEST_CHECK(!ec);

	// sleep for 3 seconds and then make sure the difference in timestamp is
	// between 2-4 seconds after touching it again
	std::this_thread::sleep_for(lt::milliseconds(3000));

	touch_file("__test_timestamp__", 10);

	file_status st2;
	stat_file("__test_timestamp__", &st2, ec);
	TEST_CHECK(!ec);

	int diff = int(st2.mtime - st1.mtime);
	std::fprintf(stderr, "timestamp difference: %d seconds. expected approx. 3 seconds\n"
		, diff);

	TEST_CHECK(diff >= 2 && diff <= 4);
}

TORRENT_TEST(directory)
{
	error_code ec;

	create_directory("file_test_dir", ec);
	if (ec) std::fprintf(stderr, "create_directory: %s\n", ec.message().c_str());
	TEST_CHECK(!ec);

	std::string cwd = current_working_directory();

	touch_file(combine_path("file_test_dir", "abc"), 10);
	touch_file(combine_path("file_test_dir", "def"), 100);
	touch_file(combine_path("file_test_dir", "ghi"), 1000);

	std::set<std::string> files;
	for (directory i("file_test_dir", ec); !i.done(); i.next(ec))
	{
		std::string f = i.file();
		TEST_CHECK(files.count(f) == 0);
		files.insert(f);
		std::fprintf(stderr, " %s\n", f.c_str());
	}

	TEST_CHECK(files.count("abc") == 1);
	TEST_CHECK(files.count("def") == 1);
	TEST_CHECK(files.count("ghi") == 1);
	TEST_CHECK(files.count("..") == 1);
	TEST_CHECK(files.count(".") == 1);
	files.clear();

	recursive_copy("file_test_dir", "file_test_dir2", ec);

	for (directory i("file_test_dir2", ec); !i.done(); i.next(ec))
	{
		std::string f = i.file();
		TEST_CHECK(files.count(f) == 0);
		files.insert(f);
		std::fprintf(stderr, " %s\n", f.c_str());
	}

	remove_all("file_test_dir", ec);
	if (ec) std::fprintf(stderr, "remove_all: %s\n", ec.message().c_str());
	remove_all("file_test_dir2", ec);
	if (ec) std::fprintf(stderr, "remove_all: %s\n", ec.message().c_str());
}

// test path functions
TORRENT_TEST(paths)
{
	TEST_EQUAL(combine_path("test1/", "test2"), "test1/test2");
	TEST_EQUAL(combine_path("test1", "."), "test1");
	TEST_EQUAL(combine_path(".", "test1"), "test1");
#ifdef TORRENT_WINDOWS
	TEST_EQUAL(combine_path("test1\\", "test2"), "test1\\test2");
	TEST_EQUAL(combine_path("test1", "test2"), "test1\\test2");
#else
	TEST_EQUAL(combine_path("test1", "test2"), "test1/test2");
#endif

#if TORRENT_USE_UNC_PATHS
	TEST_EQUAL(canonicalize_path("c:\\a\\..\\b"), "c:\\b");
	TEST_EQUAL(canonicalize_path("a\\..\\b"), "b");
	TEST_EQUAL(canonicalize_path("a\\..\\.\\b"), "b");
	TEST_EQUAL(canonicalize_path("\\.\\a"), "\\a");
	TEST_EQUAL(canonicalize_path("\\\\bla\\.\\a"), "\\\\bla\\a");
	TEST_EQUAL(canonicalize_path("c:\\bla\\a"), "c:\\bla\\a");
#endif

	TEST_EQUAL(extension("blah"), "");
	TEST_EQUAL(extension("blah.exe"), ".exe");
	TEST_EQUAL(extension("blah.foo.bar"), ".bar");
	TEST_EQUAL(extension("blah.foo."), ".");
	TEST_EQUAL(extension("blah.foo/bar"), "");

	TEST_EQUAL(remove_extension("blah"), "blah");
	TEST_EQUAL(remove_extension("blah.exe"), "blah");
	TEST_EQUAL(remove_extension("blah.foo.bar"), "blah.foo");
	TEST_EQUAL(remove_extension("blah.foo."), "blah.foo");

	TEST_EQUAL(filename("blah"), "blah");
	TEST_EQUAL(filename("/blah/foo/bar"), "bar");
	TEST_EQUAL(filename("/blah/foo/bar/"), "bar");
	TEST_EQUAL(filename("blah/"), "blah");

#ifdef TORRENT_WINDOWS
	TEST_EQUAL(is_root_path("c:\\blah"), false);
	TEST_EQUAL(is_root_path("c:\\"), true);
	TEST_EQUAL(is_root_path("\\\\"), true);
	TEST_EQUAL(is_root_path("\\\\foobar"), true);
	TEST_EQUAL(is_root_path("\\\\foobar\\"), true);
	TEST_EQUAL(is_root_path("\\\\foobar/"), true);
	TEST_EQUAL(is_root_path("\\\\foo/bar"), false);
	TEST_EQUAL(is_root_path("\\\\foo\\bar\\"), false);
#else
	TEST_EQUAL(is_root_path("/blah"), false);
	TEST_EQUAL(is_root_path("/"), true);
#endif

	// if has_parent_path() returns false
	// parent_path() should return the empty string
	TEST_EQUAL(parent_path("blah"), "");
	TEST_EQUAL(has_parent_path("blah"), false);
	TEST_EQUAL(parent_path("/blah/foo/bar"), "/blah/foo/");
	TEST_EQUAL(has_parent_path("/blah/foo/bar"), true);
	TEST_EQUAL(parent_path("/blah/foo/bar/"), "/blah/foo/");
	TEST_EQUAL(has_parent_path("/blah/foo/bar/"), true);
	TEST_EQUAL(parent_path("/a"), "/");
	TEST_EQUAL(has_parent_path("/a"), true);
	TEST_EQUAL(parent_path("/"), "");
	TEST_EQUAL(has_parent_path("/"), false);
	TEST_EQUAL(parent_path(""), "");
	TEST_EQUAL(has_parent_path(""), false);
#ifdef TORRENT_WINDOWS
	TEST_EQUAL(parent_path("\\\\"), "");
	TEST_EQUAL(has_parent_path("\\\\"), false);
	TEST_EQUAL(parent_path("c:\\"), "");
	TEST_EQUAL(has_parent_path("c:\\"), false);
	TEST_EQUAL(parent_path("c:\\a"), "c:\\");
	TEST_EQUAL(has_parent_path("c:\\a"), true);
	TEST_EQUAL(has_parent_path("\\\\a"), false);
	TEST_EQUAL(has_parent_path("\\\\foobar/"), false);
	TEST_EQUAL(has_parent_path("\\\\foobar\\"), false);
	TEST_EQUAL(has_parent_path("\\\\foo/bar\\"), true);
#endif

#ifdef TORRENT_WINDOWS
	TEST_EQUAL(is_complete("c:\\"), true);
	TEST_EQUAL(is_complete("c:\\foo\\bar"), true);
	TEST_EQUAL(is_complete("\\\\foo\\bar"), true);
	TEST_EQUAL(is_complete("foo/bar"), false);
	TEST_EQUAL(is_complete("\\\\"), true);
#else
	TEST_EQUAL(is_complete("/foo/bar"), true);
	TEST_EQUAL(is_complete("foo/bar"), false);
	TEST_EQUAL(is_complete("/"), true);
	TEST_EQUAL(is_complete(""), false);
#endif

	TEST_EQUAL(complete("."), current_working_directory());
}

// test split_string
TORRENT_TEST(split_string)
{
	char const* tags[10];
	char tags_str[] = "  this  is\ta test\t string\x01to be split  and it cannot "
		"extend over the limit of elements \t";
	int ret = split_string(tags, 10, tags_str);

	TEST_CHECK(ret == 10);
	TEST_CHECK(std::strcmp(tags[0], "this") == 0);
	TEST_CHECK(std::strcmp(tags[1], "is") == 0);
	TEST_CHECK(std::strcmp(tags[2], "a") == 0);
	TEST_CHECK(std::strcmp(tags[3], "test") == 0);
	TEST_CHECK(std::strcmp(tags[4], "string") == 0);
	TEST_CHECK(std::strcmp(tags[5], "to") == 0);
	TEST_CHECK(std::strcmp(tags[6], "be") == 0);
	TEST_CHECK(std::strcmp(tags[7], "split") == 0);
	TEST_CHECK(std::strcmp(tags[8], "and") == 0);
	TEST_CHECK(std::strcmp(tags[9], "it") == 0);

	// replace_extension
	std::string test = "foo.bar";
	replace_extension(test, "txt");
	TEST_EQUAL(test, "foo.txt");

	test = "_";
	replace_extension(test, "txt");
	TEST_EQUAL(test, "_.txt");

	test = "1.2.3/_";
	replace_extension(test, "txt");
	TEST_EQUAL(test, "1.2.3/_.txt");
}

// file class
TORRENT_TEST(file)
{
	error_code ec;
	file f;
#if TORRENT_USE_UNC_PATHS || !defined _WIN32
	TEST_CHECK(f.open("con", file::read_write, ec));
#else
	TEST_CHECK(f.open("test_file", file::read_write, ec));
#endif
	if (ec)
		std::fprintf(stderr, "open failed: [%s] %s\n", ec.category().name(), ec.message().c_str());
	TEST_EQUAL(ec, error_code());
	if (ec) std::fprintf(stderr, "%s\n", ec.message().c_str());
	file::iovec_t b = {(void*)"test", 4};
	TEST_EQUAL(f.writev(0, &b, 1, ec), 4);
	if (ec)
		std::fprintf(stderr, "writev failed: [%s] %s\n", ec.category().name(), ec.message().c_str());
	TEST_CHECK(!ec);
	char test_buf[5] = {0};
	b.iov_base = test_buf;
	b.iov_len = 4;
	TEST_EQUAL(f.readv(0, &b, 1, ec), 4);
	if (ec)
		std::fprintf(stderr, "readv failed: [%s] %s\n", ec.category().name(), ec.message().c_str());
	TEST_EQUAL(ec, error_code());
	TEST_CHECK(std::strcmp(test_buf, "test") == 0);
	f.close();
}

TORRENT_TEST(hard_link)
{
	// try to create a hard link to see what happens
	// first create a regular file to then add another link to.

	// create a file, write some stuff to it, create a hard link to that file,
	// read that file and assert we get the same stuff we wrote to the first file
	error_code ec;
	file f;
	TEST_CHECK(f.open("original_file", file::read_write, ec));
	if (ec)
		std::fprintf(stderr, "open failed: [%s] %s\n", ec.category().name(), ec.message().c_str());
	TEST_EQUAL(ec, error_code());

	file::iovec_t b = {(void*)"abcdefghijklmnopqrstuvwxyz", 26};
	TEST_EQUAL(f.writev(0, &b, 1, ec), 26);
	if (ec)
		std::fprintf(stderr, "writev failed: [%s] %s\n", ec.category().name(), ec.message().c_str());
	TEST_EQUAL(ec, error_code());
	f.close();

	hard_link("original_file", "second_link", ec);

	if (ec)
		std::fprintf(stderr, "hard_link failed: [%s] %s\n", ec.category().name(), ec.message().c_str());
	TEST_EQUAL(ec, error_code());


	TEST_CHECK(f.open("second_link", file::read_write, ec));
	if (ec)
		std::fprintf(stderr, "open failed: [%s] %s\n", ec.category().name(), ec.message().c_str());
	TEST_EQUAL(ec, error_code());

	char test_buf[27] = {0};
	b.iov_base = test_buf;
	b.iov_len = 27;
	TEST_EQUAL(f.readv(0, &b, 1, ec), 26);
	if (ec)
		std::fprintf(stderr, "readv failed: [%s] %s\n", ec.category().name(), ec.message().c_str());
	TEST_EQUAL(ec, error_code());
	TEST_CHECK(std::strcmp(test_buf, "abcdefghijklmnopqrstuvwxyz") == 0);
	f.close();

	remove("original_file", ec);
	if (ec)
		std::fprintf(stderr, "remove failed: [%s] %s\n", ec.category().name(), ec.message().c_str());

	remove("second_link", ec);
	if (ec)
		std::fprintf(stderr, "remove failed: [%s] %s\n", ec.category().name(), ec.message().c_str());
}

TORRENT_TEST(coalesce_buffer)
{
	error_code ec;
	file f;
	TEST_CHECK(f.open("test_file", file::read_write, ec));
	if (ec)
		std::fprintf(stderr, "open failed: [%s] %s\n", ec.category().name(), ec.message().c_str());
	TEST_EQUAL(ec, error_code());
	if (ec) std::fprintf(stderr, "%s\n", ec.message().c_str());
	file::iovec_t b[2] = {{(void*)"test", 4}, {(void*)"foobar", 6}};
	TEST_EQUAL(f.writev(0, b, 2, ec, file::coalesce_buffers), 4 + 6);
	if (ec)
		std::fprintf(stderr, "writev failed: [%s] %s\n", ec.category().name(), ec.message().c_str());
	TEST_CHECK(!ec);
	char test_buf1[5] = {0};
	char test_buf2[7] = {0};
	b[0].iov_base = test_buf1;
	b[0].iov_len = 4;
	b[1].iov_base = test_buf2;
	b[1].iov_len = 6;
	TEST_EQUAL(f.readv(0, b, 2, ec), 4 + 6);
	if (ec)
	{
		std::fprintf(stderr, "readv failed: [%s] %s\n"
			, ec.category().name(), ec.message().c_str());
	}
	TEST_EQUAL(ec, error_code());
	TEST_CHECK(std::strcmp(test_buf1, "test") == 0);
	TEST_CHECK(std::strcmp(test_buf2, "foobar") == 0);
	f.close();
}

