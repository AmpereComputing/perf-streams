target_sources(evp PRIVATE
  capture.cc
  count.cc
  latency.cc
  ls.cc
  occupancy.cc
  param.cc
  print.cc
  python.cc
  rate.cc
  rename.cc
  sum.cc
  summarize.cc
  time_skip.cc
)

install(DIRECTORY python DESTINATION ${INSTALL_PYTHON_PATH}/processor)
