/*
 * Assembler Main
 */

#include "config_struct.hpp"
#include "launch.hpp"
#include "logging.hpp"
#include "simple_tools.hpp"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "omni/distance_estimation.hpp"
//#include <distance_estimation.hpp>

DECL_PROJECT_LOGGER("d")

int main() {
	cfg::create_instance(debruijn::cfg_filename);

	// check config_struct.hpp parameters
	if (debruijn::K % 2 == 0) {
		FATAL("K in config.hpp must be odd!\n");
	}
	checkFileExistenceFATAL(debruijn::cfg_filename);

	// read configuration file (dataset path etc.)
	string input_dir = cfg::get().input_dir;
	string dataset = cfg::get().dataset_name;

	string work_tmp_dir = cfg::get().output_root + "tmp/";
//	std::cout << "here " << mkdir(output_dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH| S_IWOTH) << std::endl;
	mkdir(cfg::get().output_dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH | S_IWOTH);


	string genome_filename = input_dir + cfg::get().reference_genome;
	string reads_filename1 = input_dir + cfg::get().ds.first;
	string reads_filename2 = input_dir + cfg::get().ds.second;
	checkFileExistenceFATAL(genome_filename);
	checkFileExistenceFATAL(reads_filename1);
	checkFileExistenceFATAL(reads_filename2);
	INFO("Assembling " << dataset << " dataset");

	size_t insert_size = cfg::get().ds.IS;
	size_t max_read_length = 100; //CONFIG.read<size_t> (dataset + "_READ_LEN");
	int dataset_len = cfg::get().ds.LEN;
	bool paired_mode = cfg::get().paired_mode;
	bool rectangle_mode = cfg::get().rectangle_mode;
	bool etalon_info_mode = cfg::get().etalon_info_mode;
	bool from_saved = cfg::get().from_saved_graph;
	// typedefs :)
	typedef io::Reader<io::SingleRead> ReadStream;
	typedef io::Reader<io::PairedRead> PairedReadStream;
	typedef io::RCReaderWrapper<io::PairedRead> RCStream;

	// read data ('reads')

	PairedReadStream pairStream(
			std::pair<std::string, std::string>(reads_filename1,
					reads_filename2),
			insert_size);
	string real_reads = cfg::get().uncorrected_reads;
	vector<ReadStream*> reads;
	if (real_reads != "none") {
		reads_filename1 = input_dir + (real_reads + "_1");
		reads_filename2 = input_dir + (real_reads + "_2");
	}
	ReadStream reads_1(reads_filename1);
	ReadStream reads_2(reads_filename2);
	reads.push_back(&reads_1);

	reads.push_back(&reads_2);

	RCStream rcStream(&pairStream);

	// read data ('genome')
	std::string genome;
	{
		ReadStream genome_stream(genome_filename);
		io::SingleRead full_genome;
		genome_stream >> full_genome;
		genome = full_genome.GetSequenceString().substr(0, dataset_len); // cropped
	}
	// assemble it!
	INFO("Assembling " << dataset << " dataset");
	debruijn_graph::DeBruijnGraphTool<debruijn::K, RCStream>(rcStream, Sequence(genome),
			paired_mode, rectangle_mode, etalon_info_mode, from_saved,
			insert_size, max_read_length, work_tmp_dir, reads);

	unlink((cfg::get().output_root + "latest").c_str());
		if (symlink(cfg::get().output_dir_suffix.c_str(), (cfg::get().output_root + "latest").c_str())
				!= 0)
	WARN( "Symlink to latest launch failed");

	INFO("Assembling " << dataset << " dataset finished");
	// OK
	return 0;
}

