
#include "ffmpeg_video.hpp"

#include <exception>
#include <optional>
#include <vector>
#include <algorithm>
#include <functional>

#include <iostream>
#include <iomanip>
#include <sstream>

#include <cassert>

/*
	Things why this is fragile AF:

	- Does not account for frames[0].pts != 0
	- Does not account for timeBase != 1/1000
	- Does not account for ref.timeBase != src.timeBase
	- Does not account for variable frame lengths
	- Not robust against frozen sections, edit matrix runs out of space (should coalesce eq. frames)
	- Edit collection for last hunk is wonky
	- No proper parameters for frame graph, tresholds
	- Complete desync not handled

	Could/should be improved:
	- heuristics, data is usually converging
	- proper multithreading (matrix bottleneck), pin decoders to cores
	- matrix could be sliding, or sparse matrix structure (quadtree)

	TODO:
	- fix above
	- reencode src streams (subtitle needs hackery)
	- try hacks to avoid reencoding entire audio track
	- optimize for single offset scenario (container lvl track delay, no reencoding)
	- offer warning on mixed ref/src
	- much-much better error handling
	- syntehtic tests with input videos
	- figuring out wtf is going on with FFMpeg licensing
	- release
	- get disappointed noone really wants this
	- world domination?
*/

using namespace ffmpeg;

double compareFrames(const MFrame& ref, const MFrame& src) {
	if (ref->width != src->width || ref->height != src->height)
		return 1;

	const auto width = ref->width;
	const auto height = ref->height;

	uint64_t diff = 0;

	for (auto y = 0; y < height; ++y) {
		auto lineA = &ref->data[0][ref->linesize[0] * y];
		auto lineB = &src->data[0][src->linesize[0] * y];

		for (auto x = 0; x < width; ++x) {
			diff += std::abs(lineA[x] - lineB[x]);
		}
	}

	return (double)diff / width / height / (1LL << 8);
}

template<typename T>
struct Matrix2D {

	Matrix2D(size_t size = 1024 + 1) {
		resize(size, size);
	}

	T& at(size_t x, size_t y) {
		return data[x + y * width];
	}
	const T& at(size_t x, size_t y) const {
		return data[x + y * width];
	}

	void resize(size_t newWidth, size_t newHeight) {
		std::vector<T> newData{ newWidth * newHeight };

		for (auto y = 0; y < height; y++) {
			for (auto x = 0; x < width; x++) {
				newData[x + y * newWidth] = std::move(at(x, y));
			}
		}

		data = std::move(newData);

		width = newWidth;
		height = newHeight;
	}

private:
	size_t width = 0;
	size_t height = 0;

	std::vector<T> data;
};

enum class MKind : uint8_t {
	None, Match, Extra, Missing
};

using Offset = uint16_t;
using Score = int32_t;
using PTS = int64_t;

struct Cell {
	MKind kind;

	Offset offX;
	Offset offY;

	Score score;
};

struct Section {
	PTS start;
	PTS end;

	PTS len() const {
		return end - start;
	}
};

namespace {
	struct pts_t {
		PTS value;
	};

	pts_t pts(PTS value) {
		return { value };
	}

	std::ostream& operator<<(std::ostream& out, const pts_t& obj) {
		double value = obj.value / 1000.0;

		int M = (int)(value / 60) % 60;
		int S = (int)(value / 1) % 60;
		int m = (int)(std::fmod(value, 1) * 1000);

		out << std::setfill('0');
		out << std::setw(2) << M << ":";
		out << std::setw(2) << S << ".";
		out << std::setw(3) << m;

		return out;
	}

	std::ostream& operator<<(std::ostream& out, const Section& obj) {
		if (obj.start == obj.end) {
			out << "[" << pts(obj.start) << " / --:--.--- | --:--.---] ";
		} else {
			out << "[" << pts(obj.start) << " / " << pts(obj.end) << " | " << pts(obj.len()) << "] ";
		}

		return out;
	}
}

struct Match {
	MKind kind;

	Section ref;
	Section src;

	void print(std::ostream& out) {
		out << ref << src;

		switch (kind) {
			case MKind::Match: 	 out << delta(); break;
			case MKind::Extra:	 out << "+";     break;
			case MKind::Missing: out << "-";     break;
		}

		out << std::endl;
	}

	PTS delta() {
		return ref.start - src.start;
	}
};

class Matcher {

public:
	std::vector<MFrame> refValues;
	std::vector<MFrame> srcValues;

private:
	Matrix2D<Cell> matrix;

	size_t maxW = 0;
	size_t maxH = 0;

	void offer(size_t x, size_t y, MKind kind, Score score = 0) {
		Offset offX = kind != MKind::Extra;
		Offset offY = kind != MKind::Missing;

		auto& prev = matrix.at(x - offX, y - offY);

		if (prev.kind == kind) {
			offX += prev.offX;
			offY += prev.offY;
		}

		score += prev.score;

		auto& curr = matrix.at(x, y);

		if (curr.kind == MKind::None || curr.score <= score) {
			curr = { kind, offX, offY, score };
		}
	}

	void calc(size_t x, size_t y) {
		if (x != 0) {
			offer(x, y, MKind::Missing);
		}
		if (y != 0) {
			offer(x, y, MKind::Extra);
		}
		if (x != 0 && y != 0) {
			auto& ref = refValues[x - 1];
			auto& src = srcValues[y - 1];

			double diff = compareFrames(ref, src);
			Score score = lround((0.05 - diff) * 100);

			if (matrix.at(x - 1, y - 1).kind == MKind::Match && score > 0) {
				score += 1;
			}

			offer(x, y, MKind::Match, score);
		}
	}

public:
	void calc() {
		const size_t matrixW = refValues.size() +1;
		const size_t matrixH = srcValues.size() +1;

		for (size_t y = 0; y <= maxH; y++) {
			for (size_t x = maxW + 1; x < matrixW; x++) {
				calc(x, y);
			}
		}
		for (size_t y = maxH + 1; y < matrixH; y++) {
			for (size_t x = 0; x < matrixW; x++) {
				calc(x, y);
			}
		}

		maxW = matrixW - 1;
		maxH = matrixH - 1;
	}

private:
	Match cellToMatch(size_t x, size_t y) const {
		auto& cell = matrix.at(x, y);

		Match result{ cell.kind };

		if (cell.kind == MKind::Extra) {
			if (x > cell.offX +1ULL) {
				auto prev = x - cell.offX -1;
			
				result.ref = { refValues[prev].end(), refValues[prev].end() };
			}
		}
		else {
			auto start = std::max(x - cell.offX + 1, 1ULL) - 1;
			auto end = x - 1;

			result.ref = { refValues[start].start(), refValues[end].end() };
		}

		if (cell.kind == MKind::Missing) {
			if (y > cell.offY +1ULL) {
				auto prev = y - cell.offY - 1;

				result.src = { refValues[prev].end(), refValues[prev].end() };
			}
		} else {
			auto start = std::max(y - cell.offY + 1, 1ULL) - 1;
			auto end = y -1;

			result.src = { srcValues[start].start(), srcValues[end].end() };
		}

		return result;
	}

public:
	using Visitor = std::function<void(Match)>;

	void trace(Visitor visitor) const {
		auto x = maxW;
		auto y = maxH;

		while (x != 0 || y != 0) {
			visitor(cellToMatch(x, y));

			auto& cell = matrix.at(x, y);

			assert(cell.offX || cell.offY);
			assert(cell.offX <= x);
			assert(cell.offY <= y);

			x -= cell.offX;
			y -= cell.offY;
		}
	}
};

#include <thread>

#include <windows.h>

// TODO: A proper launch param lib would be nice
PTS resyncMatchTreshold = 10 * 1000;

PTS verifyMatchTreshold = 5 * 1000;
PTS verifyDriftTreshold = 100;

bool operator!=(const AVRational& objA, const AVRational& objB) {
	return objA.num != objB.num || objA.den != objB.den;
}

int main(int argc, char* argv[]) {
	{
		HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);

		DWORD dwMode;
		GetConsoleMode(hOut, &dwMode);
		SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
	}

	std::vector<std::string> inputs;

	{
		std::vector<std::string> args;

		for (auto i = 0; i < argc; i++) {
			args.emplace_back(argv[i]);
		}

		auto lastArg = args.size() -1;

		for (auto i = 1; i <= lastArg; i++) {
			auto& arg = args[i];

			if (i != lastArg) {
				if (arg == "--resync-match-treshold") {
					resyncMatchTreshold = std::atol(args[++i].data());
					continue;
				} else if (arg == "--verify-match-treshold") {
					verifyMatchTreshold = std::atol(args[++i].data());
					continue;
				} else if (arg == "--verify-drift-treshold") {
					verifyDriftTreshold = std::atol(args[++i].data());
					continue;
				}
			}

			inputs.push_back(arg);
		}

		if (inputs.size() < 2) {
			std::cerr << "Not enough input files provided" << std::endl;
			return 2;
		}
	}

	auto& out = std::cout;

	Video ref{inputs[0]};
	Video src{inputs[1]};

	if (ref.getTimeBase() != src.getTimeBase()) {
		std::cerr << "Input files have different tbase!" << std::endl;
		return 2;
	}

	// TODO: This assumption is hardcoded quite a lot..
	if (ref.getTimeBase() != AVRational{1, 1000}) {
		std::cerr << "For now only 1/1000 tbase videos are supported" << std::endl;
		return 3;
	}

	VideoGraph refScaler{ ref, "format=gray,scale=96x54" };
	VideoGraph srcScaler{ src, "format=gray,scale=96x54" };

	MFrame buffer;
	
	auto feed = [&](Matcher& matcher) {
		bool newFrames = false;

		if (ref.readWithNext(buffer) != FrameResult::END) {
			newFrames = true;

			refScaler.process(buffer);
			matcher.refValues.push_back(std::move(buffer));
		}
		if (src.readWithNext(buffer) != FrameResult::END) {
			newFrames = true;

			srcScaler.process(buffer);
			matcher.srcValues.push_back(std::move(buffer));
		}

		if (newFrames) {
			matcher.calc();
		}

		return newFrames;
	};

	std::vector<Match> finalEdits;
	std::optional<Match> sync;

	auto resync = [&] {
		sync.reset();

		Matcher matcher;
		size_t entries = 0;

		auto submit = [&](bool all) {
			std::vector<Match> edits;
			bool seenMatch = false;

			matcher.trace([&](Match entry) {
				if (entry.kind == MKind::Match) {
					seenMatch = true;
					return;
				}

				if (!all && !seenMatch) {
					return;
				}

				edits.push_back(entry);
			});

			for (auto it = edits.rbegin(); it != edits.rend(); ++it) {
				finalEdits.push_back(*it);
			}
		};

		for (auto sample = 0; sample < 1000; sample++) {
			if (!feed(matcher)) {
				submit(true);
				break;
			}

			{
				std::ostringstream buff;

				for (int i = 0; i < entries; i++) {
					buff << "\033[1A" << "\033[2K";
				}
				entries = 0;

				matcher.trace([&](Match entry) {
					entry.print(buff);
					entries++;
				});

				out << buff.str();
			}

			matcher.trace([&](Match entry) {
				if (entry.kind != MKind::Match)
					return;

				if (!sync || sync->ref.len() < entry.ref.len()) {
					sync = entry;
				}
			});

			if (sync && sync->ref.len() > resyncMatchTreshold) {
				submit(false);
				return true;
			}
		}

		return false;
	};

	auto verify = [&] {
		Matcher matcher;

		for (auto sample = 0; sample < 1000; sample++) {
			if (!feed(matcher)) {
				break;
			}

			PTS match = 0;
			PTS drift = 0;

			matcher.trace([&](Match entry) {
				if (entry.kind == MKind::Match) {
					match += entry.ref.len();
				} else {
					drift += entry.ref.len();
					drift += entry.src.len();
				}
			});

			if (match > verifyMatchTreshold) {
				return true;
			}
			if (drift > verifyDriftTreshold) {
				return false;
			}
		}

		return true;
	};
	
	PTS refEnd = ref.getDuration();

	for (auto i = 0; i < 100; i++) {
		out << "Resyncing" << std::endl;
		
		if (!resync()) {
			break;
		}

		// Binary search for sync loss
		Match anchor = *sync;

		PTS min = 0;
		PTS max = refEnd - anchor.ref.end;

		if (max - min < 2000) {
			break;
		}

		out << std::endl << "Fast forwarding" << std::endl;

		while (max - min > 2000) {
			PTS delta = (min + max) / 2;

			ref.seek(anchor.ref.end + delta);
			src.seek(anchor.src.end + delta);

			out << " " << pts(anchor.ref.end + delta);
			out << " " << pts(anchor.src.end + delta);

			if (verify()) {
				out << "  OK  " << sync->delta() << std::endl;
			
				min = delta;
			} else {
				out << " FAIL " << std::endl;
			
				max = delta;
			}
		}

		ref.seek(anchor.ref.end + min);
		src.seek(anchor.src.end + min);

		out << std::endl;
	}

	out << std::endl;
	out << "Final edits: " << std::endl;

	for (auto& edit : finalEdits) {
		edit.print(out);
	}

	return 0;
}
