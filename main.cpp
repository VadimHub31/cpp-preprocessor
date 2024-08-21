#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using filesystem::path;

const static regex reg_header(R"/(\s*#\s*include\s*"([^"]*)"\s*)/");
const static regex reg_dir(R"/(\s*#\s*include\s*<([^>]*)>\s*)/");

path operator""_p(const char* data, std::size_t sz) {
    return path(data, data + sz);
}

void SearchInIncludeDirectories(ofstream& out, const vector<path>& include_directories, smatch& match, bool& file_open);

bool PreProcess(ifstream& in, ofstream& out, const path& in_file, const vector<path>& include_directories) {
    smatch match;
    string line;
    int line_number = 0;
    while(getline(in, line)) {
        ++line_number;
        if (regex_match(line, match, reg_header)) {
            path file_path = in_file.parent_path() / string(match[1]);
            ifstream include_file(file_path); 
            if (include_file.is_open()) {
                PreProcess(include_file, out, file_path, include_directories); 
            }
            else {
                bool file_open = false;
                SearchInIncludeDirectories(out, include_directories, match, file_open);
                if (file_open == false) {
                    cout << "unknown include file "s << string(match[1]) << " at file "s << in_file.string() << " at line "s << line_number << endl;
                    return false;
                }
            }
        }
        else if (regex_match(line, match, reg_dir)) {
            bool file_open = false;
            SearchInIncludeDirectories(out, include_directories, match, file_open);
            if (file_open == false) {
                cout << "unknown include file "s << string(match[1]) << " at file "s << in_file.string() << " at line "s << line_number << endl;
                return false;
            }
        }
        else {
            out << line << endl;
        }
    }
    return true;
}

void SearchInIncludeDirectories(ofstream& out, const vector<path>& include_directories, smatch& match, bool& file_open) {
    for (const auto& include_directory : include_directories) {
        path file_path = include_directory / string(match[1]);
        ifstream include_file(file_path);
        if (include_file.is_open()) {
            file_open = true;
            PreProcess(include_file, out, file_path, include_directories);
            break;
        }
    }
}

// напишите эту функцию
bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories) {
    ifstream in_(in_file);
    if (!in_) {
        return false;
    }
    
    ofstream out_(out_file);
    if (!out_) {
        return false;
    }

    return PreProcess(in_, out_, in_file, include_directories);
}

string GetFileContents(string file) {
    ifstream stream(file);

    // конструируем string по двум итераторам
    return {(istreambuf_iterator<char>(stream)), istreambuf_iterator<char>()};
}

void Test() {
    error_code err;
    filesystem::remove_all("sources"_p, err);
    filesystem::create_directories("sources"_p / "include2"_p / "lib"_p, err);
    filesystem::create_directories("sources"_p / "include1"_p, err);
    filesystem::create_directories("sources"_p / "dir1"_p / "subdir"_p, err);

    {
        ofstream file("sources/a.cpp");
        file << "// this comment before include\n"
                "#include \"dir1/b.h\"\n"
                "// text between b.h and c.h\n"
                "#include \"dir1/d.h\"\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"
                "#   include<dummy.txt>\n"
                "}\n"s;
    }
    {
        ofstream file("sources/dir1/b.h");
        file << "// text from b.h before include\n"
                "#include \"subdir/c.h\"\n"
                "// text from b.h after include"s;
    }
    {
        ofstream file("sources/dir1/subdir/c.h");
        file << "// text from c.h before include\n"
                "#include <std1.h>\n"
                "// text from c.h after include\n"s;
    }
    {
        ofstream file("sources/dir1/d.h");
        file << "// text from d.h before include\n"
                "#include \"lib/std2.h\"\n"
                "// text from d.h after include\n"s;
    }
    {
        ofstream file("sources/include1/std1.h");
        file << "// std1\n"s;
    }
    {
        ofstream file("sources/include2/lib/std2.h");
        file << "// std2\n"s;
    }

    assert((!Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p,
                                  {"sources"_p / "include1"_p,"sources"_p / "include2"_p})));

    ostringstream test_out;
    test_out << "// this comment before include\n"
                "// text from b.h before include\n"
                "// text from c.h before include\n"
                "// std1\n"
                "// text from c.h after include\n"
                "// text from b.h after include\n"
                "// text between b.h and c.h\n"
                "// text from d.h before include\n"
                "// std2\n"
                "// text from d.h after include\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"s;

    assert(GetFileContents("sources/a.in"s) == test_out.str());
}

int main() {
    Test();
}
