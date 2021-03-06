#include <iostream>
#include <map>
#include <utility>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <omp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../common.h"

using namespace std;

#define BUF_LEN (1024)

typedef map<intptr_t, pair<size_t, uint32_t> > addr_map;

bool uniq(char *path) {
  cerr << "MEMREF: " << sizeof(MEMREF) << endl;
  struct stat sb;
  assert(stat(path, &sb) == 0);
  off_t fs = sb.st_size;
  cerr << "File size: " << fs << endl;
  assert((fs % sizeof(MEMREF)) == 0);
  size_t total_nelm = fs / sizeof(MEMREF);
  cerr << "Total number of elements: " << total_nelm << endl;
  size_t num_chunks = total_nelm / BUF_LEN;
  if (total_nelm % BUF_LEN) ++num_chunks;
  cerr << "Number of chunks: " << num_chunks << endl;
  size_t count = 0;
  addr_map *rm  = NULL;
  addr_map *wm  = NULL;
  int num_maps = 0;
  size_t bytes_read = 0;
  
#pragma omp parallel reduction(+:count, bytes_read)
  {
#pragma omp single
    {
      cerr << "Number of threads: " << omp_get_num_threads() << endl;
      rm = new addr_map[omp_get_num_threads()];
      wm = new addr_map[omp_get_num_threads()];
      num_maps = omp_get_num_threads();
    }
    int tid = omp_get_thread_num();
    MEMREF *buf = new MEMREF[BUF_LEN];
    FILE *fp = fopen(path, "rb");
#pragma omp for schedule(static)
    for (size_t i = 0; i < num_chunks; ++i) {
      fseek(fp, i * BUF_LEN * sizeof(MEMREF), SEEK_SET);
      size_t nelm = fread(buf, sizeof(MEMREF), BUF_LEN, fp);
      for (size_t ni = 0; ni < nelm; ++ni) {
        const MEMREF &mr = buf[ni];
        if (mr.type == TRACE_FUNC_CALL) {
          cerr << "Call to " << mr.addr << endl;
        } else if (mr.type == TRACE_FUNC_RET) {
          cerr << "Retrun from " << mr.addr << endl;          
        } else if (mr.type == TRACE_READ) {
          pair<addr_map::iterator, bool> ret = rm[tid].insert(
              make_pair(mr.addr, make_pair(1, mr.size)));
          if (!ret.second) {
            ++ret.first->second.first;
            ret.first->second.second =
                std::max(ret.first->second.second, mr.size);
          }
          bytes_read += mr.size;
          
        }
        ++count;
      }
    }
  }
  
  cout << "Number of elements processed: " << count << endl;
  cout << "Total bytes read: " << bytes_read << endl;  

  for (int i = 1; i < num_maps; ++i) {
    addr_map::iterator it = rm[i].begin();
    addr_map::iterator it_end = rm[i].end();
    for (; it != it_end; ++it) {
      pair<addr_map::iterator, bool> ret = rm[0].insert(*it);
      if (!ret.second) {
        ret.first->second.first += it->second.first;
        ret.first->second.second = max(ret.first->second.second,
                it->second.second);
      }
    }
  }

  cout << "Number of uniq addresses: " << rm[0].size() << endl;

  addr_map::iterator it = rm[0].begin();
  addr_map::iterator it_end = rm[0].end();
  size_t uniq_read_bytes = 0;
  size_t dup_read_bytes = 0;
  for (; it != it_end; ++it) {
    uniq_read_bytes += it->second.second;
    dup_read_bytes += it->second.first * it->second.second;
  }
  cout << "Total uniq bytes read: " << uniq_read_bytes << endl;
  cout << "Total dup bytes read: " << dup_read_bytes << endl;
  

  return true;  
}

int main(int argc, char *argv[]) {
  char *file_path = argv[1];
  if (!uniq(file_path)) return EXIT_FAILURE;
  return EXIT_SUCCESS;
}

