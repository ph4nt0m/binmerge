#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <array>
#include <algorithm>

#include "docopt.h"

/******************************************************************************/

struct MatchResult
{
  // Results of the pattern search
  bool patternFound = false;
  std::size_t matchPosition = 0; // position of the first byte of the match
  std::size_t patternSize = 0;

  // Results of the byte-wise comparison of the overlapping area
  std::size_t bytesDiffering = 0;

  // Some useful methods
  std::size_t overlapCount() const
  {
    return matchPosition + patternSize;
  }

  double quota() const
  {
    if (!patternFound || overlapCount() == 0)
      return 0.0;
    else
      return static_cast<double>(overlapCount()-bytesDiffering) / overlapCount();
  }
};

MatchResult searchInFile(std::istream& file, std::vector<unsigned char>& pattern, std::streampos pos = 0)
{
  constexpr std::size_t blockSize = 4096;

  // Allocate "rolling" buffer
  std::array<unsigned char, 2 * blockSize> buffer;

  // Read first block
  file.clear();
  file.seekg(pos);
  file.read(reinterpret_cast<char*>(&buffer[0]), blockSize);
  std::size_t bytesReadPreviously = file.gcount();

  // Sanity check (less bytes than requested despite no eof)
  if (bytesReadPreviously < blockSize && !file.eof())
    throw std::system_error();

  std::size_t realBufferSize = bytesReadPreviously;
  std::size_t position = pos;

  while (file || realBufferSize >= pattern.size())
  {
    // Pre-read next block and append to current block
    file.read(reinterpret_cast<char*>(&buffer[bytesReadPreviously]), blockSize);
    std::size_t bytesRead = file.gcount();

    // Sanity check (less bytes than requested despite no eof)
    if (bytesRead < blockSize && !file.eof())
      throw std::system_error();

    // Calculate the buffer's current fill level
    realBufferSize = bytesReadPreviously + bytesRead;

    // Define range of the buffer that will be searched
    // The first byte of the searched pattern has to lie in the first half
    auto start = &buffer[0];
    auto stop  = &buffer[std::min(bytesReadPreviously + pattern.size() - 1, realBufferSize)];

    // Perform search within specified range
    auto result = std::search(start, stop, pattern.begin(), pattern.end());
    if (result != stop)
      return MatchResult{true, position + std::distance(start, result), pattern.size()};

    // Shift pre-read block to the beginning of the buffer
    std::copy(&buffer[bytesReadPreviously], &buffer[buffer.size()], &buffer[0]);
    position += bytesReadPreviously;

    bytesReadPreviously = bytesRead;
  }

  return MatchResult{};
}

/******************************************************************************/

std::size_t compareFiles(std::istream& file1, std::istream& file2)
{
  constexpr std::size_t blockSize = 4096;

  // Allocate buffers
  std::array<unsigned char, blockSize> buffer1, buffer2;

  std::size_t bytesTotal = 0, bytesDifferent = 0;

  do
  {
    // Read next blocks
    file1.read(reinterpret_cast<char*>(&buffer1[0]), blockSize);
    file2.read(reinterpret_cast<char*>(&buffer2[0]), blockSize);

    std::size_t bytesRead1 = file1.gcount();
    std::size_t bytesRead2 = file2.gcount();

    // Sanity check (less bytes than requested despite no eof)
    if ((bytesRead1 < blockSize && !file1.eof()) ||
        (bytesRead2 < blockSize && !file2.eof()))
      throw std::system_error();

    // Compare as many bytes as possible
    auto numberOfBytes = std::min(bytesRead1, bytesRead2);
    bytesTotal += numberOfBytes;

    // Count differences
    for (std::size_t i = 0; i < numberOfBytes; ++i)
      if (buffer1[i] != buffer2[i])
        ++bytesDifferent;

  } while (file1 && file2);

  return bytesDifferent;
}

/******************************************************************************/

std::string getFilename(const std::string& path)
{
  return path.substr(path.find_last_of("\\") + 1)
             .substr(path.find_last_of("/")  + 1);
}

/******************************************************************************/

void printResults(const std::vector<std::string>& fileNames,
                  const std::vector<MatchResult>& searchResults)
{
  std::cout << "Summary:\n";
  for (int i = 0; i < fileNames.size(); ++i)
  {
    std::cout << "File " << i+1 << ": " << getFilename(fileNames[i]) << '\n';

    // Skip last item after printing file name
    if (i == fileNames.size()-1)
      break;

    std::cout << " |-> ";
    std::size_t overlap = searchResults[i].overlapCount();
    float quota = 100.0 * searchResults[i].quota();

    if (searchResults[i].patternFound)
      std::cout << "overlap " << quota << "% (out of " << overlap << " bytes)" << '\n';
    else
      std::cout << "no match" << '\n';
  }
}

/******************************************************************************/

void mergeFiles(const std::vector<std::string>& fileNames,
                const std::vector<MatchResult>& searchResults,
                const std::string& outputFileName)
{
    // Create output file and
    std::ofstream outputFile(outputFileName, std::ios::binary);
    if(!outputFile)
    {
        std::cerr << "File: " << outputFileName << " failed to open." << '\n';
        return;
    }

    for (int i = 0; i < fileNames.size(); ++i)
    {
      std::ifstream inputFile(fileNames[i], std::ios::binary);

      // Basic sanity check
      if (!inputFile)
      {
        std::cerr << "File: " << fileNames[i] << " failed to open." << '\n';
        return;
      }

      // If pattern was found in this file, skip the overlapping part
      // The first file will always be copied entirely since it has no predecessor
      if (i > 0 && searchResults[i-1].patternFound)
      {
        auto seekPosition = searchResults[i-1].overlapCount();
        inputFile.seekg(seekPosition);
      }

      // Copy from current position until the end
      outputFile << inputFile.rdbuf();
    }
}

/******************************************************************************/

int main(int argc, char* argv[])
{
  const char USAGE[] =
  R"(Merge binary files with possible overlap.

Usage:
  binmerge [options] [--] <file> <file>...

Options:
  -h --help               Show this screen.
  --version               Show version.
  -b, --best              Perform continuous search to find best match.
  -o FILE, --output FILE  Output file [default: output.bin].
  )";

  auto args = docopt::docopt(USAGE, {argv+1, argv+argc}, true, "binmerge 0.2.0");

  //for(auto const& arg : args)
  //  std::cout << arg.first <<  arg.second << '\n';

  auto fileNames = args["<file>"].asStringList();

  // Open first file
  std::ifstream file1(fileNames[0], std::ios::binary);

    // Basic sanity check
  if (!file1)
  {
    std::cerr << "File: " << fileNames[0] << " failed to open." << '\n';
    return 1;
  }

  std::vector<MatchResult> searchResults;

  for (int i = 1; i < fileNames.size(); ++i)
  {
    // Extract last 20 bytes
    file1.clear();
    file1.seekg(-20, std::ios_base::end);

    std::vector<unsigned char> pattern(
      (std::istreambuf_iterator<char>(file1)),
      (std::istreambuf_iterator<char>())
    );

    // Print pattern for debugging purposes
    std::cout << "Looking for byte pattern in file " << getFilename(fileNames[i]) << ":\n";
    for (std::size_t i = 0; i < pattern.size(); ++i)
    {
      std::cout << std::hex << std::setfill('0') << std::setw(2)
                << static_cast<unsigned int>(pattern[i]) << " ";
    }
    std::cout << std::dec << std::setfill(' ') << "\n";

    // Open next file
    std::ifstream file2(fileNames[i], std::ios::binary);

    // Basic sanity check
    if (!file2)
    {
      std::cerr << "File: " << fileNames[i] << " failed to open." << '\n';
      return 1;
    }

    // Search pattern in second file
    MatchResult result;
    MatchResult lastResult = searchInFile(file2, pattern);

    // Continue search, remembering best match
    while (lastResult.patternFound)
    {
      // Clear any stream flags
      file1.clear();
      file2.clear();

      // Position file pointers accordingly
      file1.seekg(-lastResult.overlapCount(), std::ios_base::end);
      file2.seekg(0);

      // Peform a bytewise comparison of the potentially overlapping area
      lastResult.bytesDiffering = compareFiles(file1, file2);

      // Take this one if quota is higher
      if (lastResult.quota() > result.quota())
        result = lastResult;

      // Abort if quota is sufficiently high (TODO: make this a user setting)
      if (result.quota() > 0.7 || !args["--best"].asBool())
        break;

      // Continue from last match position
      auto previousMatchPos = lastResult.matchPosition;
      lastResult = searchInFile(file2, pattern, previousMatchPos+1);
    }

    searchResults.push_back(result);

    if(!result.patternFound)
    {
      std::cout << "Pattern not found\n";
    }
    else
    {
      std::cout << "Found pattern at position " << std::hex
                << result.matchPosition << std::dec << '\n'
                << "Overlap match quota: " << std::fixed << std::setprecision(2)
                << 100.0 * result.quota() << "% ("
                << result.bytesDiffering << " out of "
                << result.overlapCount() << " bytes differ)\n";
    }

    std::cout << "---------\n";

    file1.swap(file2); // alternatively, file1 = std::move(file2)
  }

  file1.close();

  printResults(fileNames, searchResults);

  std::cout << "\nMatching files will be merged accordingly (regardless of quota),\n"
            << "while non-matching files will simply be concatenated.\n";

  // Merge files if requested
  char decision;
  std::cout << "Merge files (y/n)? ";
  std::cin >> decision;

  if (decision == 'y' || decision == 'Y')
    mergeFiles(fileNames, searchResults, args["--output"].asString());

  return 0;
}
