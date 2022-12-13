#include <iostream>
#include <regex>

int main() {
  
  // std::string raw("^((?!(\\/)\\.).)*$");

  std::string raw("^(\\/tmp\\/k028ma9g29i\\/test2t71u8blfak5\\/ignore(\\/(?!\\.)(((?!(\\/)\\.).)*?)|$))$");
  // std::string raw("^(?:\\/tmp\\/k028ma9g29i\\/test2t71u8blfak5\\/(?!\\.)(?=.)[^/]*?\\.ignore)$");


  // micromatch
  // std::string raw("^(?:(?!\\.)(?:(?:(?!(?:^|\\/)\\.).)*?)\\/?)$");

  std::cout << raw << std::endl;
  std::regex rx(raw);
  std::cout << std::regex_match("/tmp/k028ma9g29i/test2t71u8blfak5/test.txt", rx) << std::endl;
  std::cout << std::regex_match("/tmp/k028ma9g29i/test2t71u8blfak5/test.ignore", rx) << std::endl;
  std::cout << std::regex_match("/tmp/k028ma9g29i/test2t71u8blfak5/ignore/test.ignore", rx) << std::endl;
  std::cout << std::regex_match("/tmp/k028ma9g29i/test2t71u8blfak5/ignore/test.txt", rx) << std::endl;
}