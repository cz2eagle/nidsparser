/*
 * Copyright (c) 2015 Sergi Granell (xerpi)
 */

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <list>
#include <map>
#include <string>
#include <sys/types.h>
#include <dirent.h>
using namespace std;

typedef uint32_t NID;

struct Import {
	NID nid;
};

struct Module {
	NID nid;
	map <string, Import> impmap;
};

struct Library {
	NID nid;
	map <string, Module> modmap;
};

class NID_Database {
private:
	map <string, Library> libmap;

	int parse_stub_file(const char *file)
	{
		FILE *fp;
		size_t n_read;
		size_t filesize;
		char *filedata;
		char *cursor;
		char *import_line;
		NID mod_NID;
		char mod_name[128];
		NID imp_NID;
		char imp_name[128];

		//printf("Parsing file: %s\n", file);

		fp = fopen(file, "r");
		if (!fp) {
			fprintf(stderr, "Error opening %s\n", file);
			goto exit_error;
		}

		/* Read whole file */
		fseek(fp, 0, SEEK_END);
		filesize = ftell(fp);
		fseek(fp, 0, SEEK_SET);

		filedata = (char *)malloc(filesize);
		if (!filedata) {
			fprintf(stderr, "Error allocating memory\n");
			goto exit_close;
		}

		n_read = fread(filedata, 1, filesize, fp);
		if (n_read != filesize) {
			fprintf(stderr, "Error reading from file\n");
			goto exit_free;
		}

		cursor = filedata;

		/* Find PSP2_IMPORT_HEAD */
		if ((import_line = strstr(cursor, "PSP2_IMPORT_HEAD")) != NULL) {
			sscanf(import_line, "%*s %X, %[^,]s", &mod_NID, mod_name);
			printf("Module name: %-32s NID: 0x%08X\n", mod_name, mod_NID);
			add_Library(mod_NID, mod_name);
		}

		/* Add module with the same name */
		add_Module(mod_name, mod_NID, mod_name);

		cursor = import_line + 1;
		while ((import_line = strstr(cursor, "PSP2_IMPORT_FUNC")) != NULL) {
			sscanf(import_line, "%*s %*s %*s %*s %X, %s", &imp_NID, imp_name);
			printf("Import name: %-32s NID: 0x%08X\n", imp_name, imp_NID);

			add_Import(mod_name, mod_name, imp_NID, imp_name);

			cursor = import_line + 1;
		}


		free(filedata);
		fclose(fp);
		return 1;
	exit_free:
		free(filedata);
	exit_close:
		fclose(fp);
	exit_error:
		return 0;
	}

public:
	NID_Database() {}

	int gen_NID_database(const char *dir)
	{
		DIR *dp;
		struct dirent *ep;
		char path[1024];

		dp = opendir(dir);
		if (dp == NULL) {
			fprintf(stderr, "Error opening %s\n", dir);
			goto exit_error;
		}

		while ((ep = readdir(dp))) {
			if (strcmp(ep->d_name, ".") == 0 || strcmp(ep->d_name, "..") == 0) {
				continue;
			}

			sprintf(path, "%s/%s", dir, ep->d_name);
			if (ep->d_type == DT_DIR) {
				gen_NID_database(path);
			} else {
				/* Check extension */
				const char *ext = strrchr(path, '.');
				if (ext) {
					if (strcmp(".S", ext) == 0 || strcmp(".s", ext) == 0) {
						parse_stub_file(path);
					}
				}
			}
		}


		closedir(dp);
		return 1;
	exit_error:
		return 0;
	}


	int write_json(const char *file)
	{
		FILE *fp;
		map <string, Library>::iterator lib_it;
		map <string, Module>::iterator mod_it;
		map <string, Import>::iterator imp_it;

		fp = fopen(file, "w");
		if (!fp) {
			fprintf(stderr, "Error opening %s\n", file);
			goto exit_error;
		}

		fprintf(fp, "{\n");

		/* For each Library */
		for (lib_it = libmap.begin(); lib_it != libmap.end(); lib_it++) {

			fprintf(fp, "\t\"%s\": {\n", lib_it->first.c_str());
			fprintf(fp, "\t\t\"nid\": %u,\n", lib_it->second.nid);
			fprintf(fp, "\t\t\"modules\": {\n");

			Library &lib = lib_it->second;

			/* For each module */
			for (mod_it = lib.modmap.begin(); mod_it != lib.modmap.end(); mod_it++) {

				fprintf(fp, "\t\t\t\"%s\": {\n", mod_it->first.c_str());
				fprintf(fp, "\t\t\t\t\"nid\": %u,\n", mod_it->second.nid);
				fprintf(fp, "\t\t\t\t\"kernel\": false,\n");
				fprintf(fp, "\t\t\t\t\"functions\": {\n");

				Module &mod = mod_it->second;

				/* For each import */
				for (imp_it = mod.impmap.begin(); imp_it != mod.impmap.end(); imp_it++) {

					fprintf(fp, "\t\t\t\t\t\"%s\": %u,\n",
						imp_it->first.c_str(),
						imp_it->second.nid);

				}

				fprintf(fp, "\t\t\t},\n");

			}

			fprintf(fp, "\t\t},\n");
			fprintf(fp, "\t},\n");
		}

		fprintf(fp, "}\n");
		fflush(fp);
		fclose(fp);
		return 1;
	exit_error:
		return 0;
	}

	void add_Import(string lib_name, string mod_name, NID nid, string imp_name)
	{
		/* Find library */
		map <string, Library>::iterator lib_it;
		lib_it = libmap.find(lib_name);
		if (lib_it == libmap.end()) {
			return;
		}

		Library &lib = lib_it->second;

		/* Find module */
		map <string, Module>::iterator mod_it;
		mod_it = lib.modmap.find(mod_name);
		if (mod_it == lib.modmap.end()) {
			return;
		}

		Module &mod = mod_it->second;

		if (mod.impmap.find(imp_name) == mod.impmap.end()) {
			Import imp;
			imp.nid = nid;
			mod.impmap.insert(pair<string, Import>(imp_name, imp));
		}
	}

	void add_Module(string lib_name, NID nid, string mod_name)
	{
		/* Find library */
		map <string, Library>::iterator it;
		it = libmap.find(lib_name);
		if (it == libmap.end()) {
			return;
		}

		Library &lib = it->second;

		if (lib.modmap.find(mod_name) == lib.modmap.end()) {
			Module mod;
			mod.nid = nid;
			lib.modmap.insert(pair<string, Module>(mod_name, mod));
		}
	}

	void add_Library(NID nid, string name)
	{
		if (libmap.find(name) == libmap.end()) {
			Library lib;
			lib.nid = nid;
			libmap.insert(pair<string, Library>(name, lib));
		}
	}
};

static void usage();

int main(int argc, char *argv[])
{
	NID_Database nid_db;

	if (argc < 2) {
		usage();
		goto exit_error;
	}


	if (nid_db.gen_NID_database(argv[1]) == 0) {
		fprintf(stderr, "Error generating the NID database\n");
		goto exit_error;
	}

	nid_db.write_json("db.json");

	return EXIT_SUCCESS;

exit_error:
	return EXIT_FAILURE;
}

void usage()
{
	fprintf(stderr,
		"nidsparser by xerpi\n"
		"Usage:\n"
		"\t./nidsparser stubs_folder\n\n");
}
