// Copyright 2017 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "environment.h"

#ifdef SYCL_LANGUAGE_VERSION
#include "sycl/sycl.hpp"
#endif

OSPRayEnvironment::OSPRayEnvironment(int argc, char **argv)
{
  ParseArgs(argc, argv);
}

void OSPRayEnvironment::ParseArgs(int argc, char **argv)
{
  std::vector<std::string> testArgs;
  for (int idx = 0; idx < argc; ++idx) {
    testArgs.push_back(argv[idx]);
  }
  for (size_t idx = 0; idx < testArgs.size(); ++idx) {
    if (testArgs.at(idx) == "--help") {
      std::cout << "--help : display this help msg\n"
                << "--dump-img : dump the rendered image to file\n"
                << "--imgsize-x=XX : change the length of an image\n"
                << "--imgsize-y=XX : change the height of an image\n"
                << "--renderer-type=XX : change the renderer used for tests\n"
                << "--baseline-dir=XX : Change the directory used for baseline "
                   "images during tests\n"
                << "--own-SYCL : turn on SYCL data sharing.\n";

      std::exit(EXIT_SUCCESS);
    } else if (testArgs.at(idx) == "--dump-img") {
      dumpImg = true;
    } else if (testArgs.at(idx).find("--renderer-type=") == 0) {
      rendererType = GetStrArgValue(&testArgs.at(idx));
    } else if (testArgs.at(idx).find("--imgsize-x=") == 0) {
      imgSize.x = GetNumArgValue(&testArgs.at(idx));
    } else if (testArgs.at(idx).find("--imgsize-y=") == 0) {
      imgSize.y = GetNumArgValue(&testArgs.at(idx));
    } else if (testArgs.at(idx).find("--baseline-dir=") == 0) {
      baselineDir = GetStrArgValue(&testArgs.at(idx));
    } else if (testArgs.at(idx).find("--failed-dir=") == 0) {
      failedDir = GetStrArgValue(&testArgs.at(idx));
    } else if (testArgs.at(idx).find("--own-SYCL") == 0) {
#ifdef SYCL_LANGUAGE_VERSION
      ownSYCL = true;
#else
      std::cerr << "WARNING: Compiled without SYCL support, "
                   "SYCL data sharing not supported."
                << std::endl;
#endif
    }
  }
}

int OSPRayEnvironment::GetNumArgValue(std::string *arg) const
{
  int ret = 0;
  size_t valueStartPos = arg->find_first_of('=');
  if (valueStartPos != std::string::npos) {
    ret = std::stoi(arg->substr(valueStartPos + 1));
    if (ret <= 0) {
      std::cout << "Incorrect value specified for argument %s " << *arg
                << std::endl
                << std::flush;
    }
  }
  return ret;
}

std::string OSPRayEnvironment::GetStrArgValue(std::string *arg) const
{
  std::string ret;
  size_t valueStartPos = arg->find_first_of('=');
  if (valueStartPos != std::string::npos) {
    ret = arg->substr(valueStartPos + 1);
  }
  return ret;
}

#ifdef SYCL_LANGUAGE_VERSION
OSPRayEnvironment::~OSPRayEnvironment()
{
  delete appsSyclQueue;
}

sycl::queue *OSPRayEnvironment::GetAppsSyclQueue()
{
  if (ownSYCL && !appsSyclQueue)
    appsSyclQueue = new sycl::queue;
  return appsSyclQueue;
}
#endif
