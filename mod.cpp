/**
Copyright (c) 2013, Philip Deegan.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

    * Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
copyright notice, this list of conditions and the following disclaimer
in the documentation and/or other materials provided with the
distribution.
    * Neither the name of Philip Deegan nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include "maiken/module/init.hpp"
#include <json/json.h>
#include <json/writer.h>

namespace mkn {
// "settings":
// {
//      "sublimegdb_executables":
//      {
//          "first_executable_name":
//          {
//              "workingdir": "${folder:${project_path:first_executable_name}}",
//              "commandline": "gdb --interpreter=mi ./first_executable"
//          },
//          "second_executable_name":
//          {
//              "workingdir":
//              "${folder:${project_path:second_executable_name}}",
//              "commandline": "gdb --interpreter=mi ./second_executable"
//          }
//      }
// }

class SublimeGDBModule : public maiken::Module {
 private:
 protected:
  static void VALIDATE_NODE(const YAML::Node &node) {
    using namespace kul::yaml;
    Validator({NodeValidator("project_file")}).validate(node);
  }

  static std::string FIND_FILE(maiken::Application &a) {
    KLOG(INF) << kul::env::CWD();
    kul::File file(".sublime-project");
    if (file) return file.real();
    return "";
  }

 public:
  void link(maiken::Application &a, const YAML::Node &node) KTHROW(std::exception) override {
    if (a.getMain().empty()) return;
    VALIDATE_NODE(node);
    std::string projectFile = "";
    if (node["project_file"])
      projectFile = node["project_file"].Scalar();
    else
      projectFile = FIND_FILE(a);
    if (projectFile.empty())
      KEXIT(1,
            "MKN plugin: subl.gdb failed to find appropriate project file to "
            "continue");
    Json::CharReaderBuilder rBuilder;
    Json::Value root;
    {
      kul::io::Reader inFile(projectFile);
      std::string errs;
      bool parsingSuccessful = Json::parseFromStream(rBuilder, inFile.buffer(), &root, &errs);
      if (!parsingSuccessful)
        KEXIT(1, "MKN plugin: subl.gdb failed to parse JSON project file: ") << projectFile << " \n"
                                                                             << errs;
    }

    checkTarget(root["settings"]["sublimegdb_executables"], a);

    Json::StreamWriterBuilder builder;
    builder["commentStyle"] = "None";
    builder["indentation"] = "  ";
    std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
    std::ofstream outputFileStream(projectFile);
    writer->write(root, &outputFileStream);
  }

  void checkTarget(Json::Value &exes, maiken::Application const &target) {
    if (!exes[target.baseLibFilename()]) createTarget(exes, target);
    checkLDPath(exes[target.baseLibFilename()], target);
  }

  void checkLDPath(Json::Value &exe, maiken::Application const &target) {
    std::string arg;
    std::vector<std::string> paths, befores;
    std::vector<std::pair<std::string, std::string> > envies;

    for (const auto &s : target.libraryPaths())
      if (std::find(paths.begin(), paths.end(), s) == paths.end()) paths.emplace_back(s);
    for (const auto &s : paths) arg += s + kul::env::SEP();
    if (!arg.empty()) arg.pop_back();
    auto doVar = [&](std::string var) {
      if (kul::env::EXISTS(var.c_str()))
        for (auto str : kul::String::SPLIT(kul::env::GET(var.c_str()), kul::env::SEP()))
          befores.emplace_back(str);
      kul::cli::EnvVar pa(var, arg, kul::cli::EnvVarMode::PREP);
      envies.emplace_back(pa.name(), pa.toString());
    };
#if defined(__APPLE__)
    doVar("DYLD_LIBRARY_PATH");
#endif
#ifdef _WIN32
    doVar("PATH");
#else
    doVar("LD_LIBRARY_PATH");
#endif
    for (auto &ev : envies) {
      for (auto str : kul::String::SPLIT(ev.second, kul::env::SEP()))
        if (ev.second.find(str) != std::string::npos && ev.second.find(str) != ev.second.rfind(str))
          kul::String::REPLACE(ev.second, str + kul::env::SEP(), "");
      while (ev.second.find("::") != std::string::npos) kul::String::REPLACE(ev.second, "::", ":");
    }

    for (auto const &ev : envies) exe["env"][ev.first] = ev.second;
  }

  void createTarget(Json::Value &exes, maiken::Application const &target) {
    auto exe = target.baseLibFilename();
    exes[exe]["workingdir"] = "${folder:${project_path:mkn.yaml}}/bin/" + target.buildDir().name();
    exes[exe]["commandline"] = "gdb --interpreter=mi ./" + exe;
  }
};

}  // namespace mkn

extern "C" KUL_PUBLISH maiken::Module *maiken_module_construct() {
  return new mkn::SublimeGDBModule;
}

extern "C" KUL_PUBLISH void maiken_module_destruct(maiken::Module *p) { delete p; }
