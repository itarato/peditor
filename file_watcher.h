#pragma once

#include <errno.h>
#include <fcntl.h>
#include <sys/inotify.h>
#include <unistd.h>

#include <string>

#include "debug.h"
#include "utility.h"

using namespace std;

#define FILE_WATCHER_NAME_MAX 256
#define FILE_WATCHER_INOTIFY_BUF_LEN \
  (10 * (sizeof(struct inotify_event) + FILE_WATCHER_NAME_MAX + 1))

struct FileWatcher {
  FileWatcher() {}

  bool watch(string filePathArg) {
    if (fd != -1 && wd != -1) {
      if (inotify_rm_watch(fd, wd) == -1)
        reportAndExit("Cannot remove file monitoring");

      fd = -1;
    }

    fd = inotify_init();
    if (fd == -1) reportAndExit("Cannot init inotify");

    int flags = fcntl(fd, F_GETFL);
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
      reportAndExit("Cannot make file watch event read non blocking");
    }

    filePath = filePathArg;
    wd = inotify_add_watch(fd, filePathArg.c_str(), IN_MODIFY);
    if (wd == -1) reportAndExit("Cannot add watcher");

    return true;
  }

  bool hasBeenModified() {
    if (fd == -1) return false;

    char buf[FILE_WATCHER_INOTIFY_BUF_LEN] __attribute__((aligned(8)));

    int readLen = read(fd, buf, FILE_WATCHER_INOTIFY_BUF_LEN);
    if (readLen == -1) {
      if (errno != EAGAIN) {
        reportAndExit("Failed reading events");
      } else {
        return false;
      }
    } else if (readLen == 0) {
      reportAndExit("Invalid length (0) for event read");
    }

    char *p;
    for (p = buf; p < buf + readLen;) {
      struct inotify_event *event = (struct inotify_event *)p;
      p += sizeof(struct inotify_event) + event->len;

      if ((event->mask & IN_MODIFY) ? "modify" : "not modify") {
        return true;
      }
    }

    return false;
  }

  void ignoreEventCycle() { hasBeenModified(); }

 private:
  int fd{-1};
  int wd{-1};
  string filePath{};
};
