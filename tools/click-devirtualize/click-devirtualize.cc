/*
 * click-devirtualize.cc -- virtual function eliminator for Click routers
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include <click/pathvars.h>

#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include "lexert.hh"
#include "routert.hh"
#include "toolutils.hh"
#include "elementmap.hh"
#include "cxxclass.hh"
#include <click/archive.hh>
#include "specializer.hh"
#include "signature.hh"
#include <click/clp.h>
#include <click/driver.hh>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>

#define HELP_OPT		300
#define VERSION_OPT		301
#define CLICKPATH_OPT		302
#define ROUTER_OPT		303
#define EXPRESSION_OPT		304
#define OUTPUT_OPT		305
#define KERNEL_OPT		306
#define USERLEVEL_OPT		307
#define SOURCE_OPT		308
#define CONFIG_OPT		309
#define NO_DEVIRTUALIZE_OPT	310
#define DEVIRTUALIZE_OPT	311
#define INSTRS_OPT		312
#define REVERSE_OPT		313

static Clp_Option options[] = {
  { "clickpath", 'C', CLICKPATH_OPT, Clp_ArgString, 0 },
  { "config", 'c', CONFIG_OPT, 0, Clp_Negate },
  { "devirtualize", 0, DEVIRTUALIZE_OPT, Clp_ArgString, Clp_Negate },
  { "expression", 'e', EXPRESSION_OPT, Clp_ArgString, 0 },
  { "file", 'f', ROUTER_OPT, Clp_ArgString, 0 },
  { "help", 0, HELP_OPT, 0, 0 },
  { 0, 'n', NO_DEVIRTUALIZE_OPT, Clp_ArgString, 0 },
  { "kernel", 'k', KERNEL_OPT, 0, Clp_Negate },
  { "instructions", 'i', INSTRS_OPT, Clp_ArgString, 0 },
  { "output", 'o', OUTPUT_OPT, Clp_ArgString, 0 },
  { "reverse", 'r', REVERSE_OPT, 0, Clp_Negate },
  { "source", 's', SOURCE_OPT, 0, Clp_Negate },
  { "user", 'u', USERLEVEL_OPT, 0, Clp_Negate },
  { "version", 'v', VERSION_OPT, 0, 0 },
};

static const char *program_name;


String
click_to_cxx_name(const String &click)
{
  StringAccum sa;
  const char *s = click.data();
  const char *end_s = s + click.length();
  for (; s < end_s; s++)
    if (*s == '_')
      sa << "_u";
    else if (*s == '@')
      sa << "_a";
    else if (*s == '/')
      sa << "_s";
    else
      sa << *s;
  return sa.take_string();
}

String
specialized_click_name(ElementT *e)
{
  return e->type_name() + "@@" + e->name();
}

static void
parse_instruction(const String &text, Signatures &sigs,
		  ErrorHandler *errh)
{
  Vector<String> words;
  cp_spacevec(text, words);

  if (words.size() == 0 || words[0].data()[0] == '#')
    /* nada */;
  else if (words[0] == "like") {
    if (words.size() < 3)
      errh->error("too few arguments to `like'");
  } else if (words[0] == "noclass") {
    if (words.size() < 2)
      errh->error("too few arguments to `noclass'");
    for (int i = 1; i < words.size(); i++)
      sigs.specialize_class(words[i], 0);
  } else
    errh->error("unknown command `%s'", words[0].cc());
}

static void
parse_instruction_file(const char *filename, Signatures &sigs,
		       ErrorHandler *errh)
{
  String text = file_string(filename, errh);
  const char *s = text.data();
  int pos = 0;
  int len = text.length();
  while (pos < len) {
    int pos1 = pos;
    while (pos < len && s[pos] != '\n' && s[pos] != '\r')
      pos++;
    parse_instruction(text.substring(pos1, pos - pos1), sigs, errh);
    while (pos < len && (s[pos] == '\n' || s[pos] == '\r'))
      pos++;
  }
}

static void
reverse_transformation(RouterT *r, ErrorHandler *)
{
  // parse fastclassifier_config
  if (r->archive_index("devirtualize_info") < 0)
    return;
  ArchiveElement &fc_ae = r->archive("devirtualize_info");
  Vector<String> new_click_names, old_click_names;
  parse_tabbed_lines(fc_ae.data, &new_click_names, &old_click_names, (void *)0);

  // prepare type_index_map : type_index -> configuration #
  HashMap<int, int> new_uid_map(-1);
  Vector<ElementClassT *> old_class;
  for (int i = 0; i < new_click_names.size(); i++) {
    new_uid_map.insert(r->get_type(new_click_names[i])->uid(), old_class.size());
    old_class.push_back(ElementClassT::default_class(old_click_names[i]));
  }

  // change configuration
  for (int i = 0; i < r->nelements(); i++) {
    ElementT *e = r->element(i);
    int nnm = new_uid_map[e->type_uid()];
    if (nnm >= 0)
      e->set_type(old_class[nnm]);
  }

  // remove requirements
  {
    Vector<String> requirements = r->requirements();
    for (int i = 0; i < requirements.size(); i++)
      if (requirements[i].substring(0, 12) == "devirtualize")
	r->remove_requirement(requirements[i]);
  }
  
  // remove archive elements
  for (int i = 0; i < r->narchive(); i++) {
    ArchiveElement &ae = r->archive(i);
    if (ae.name.substring(0, 12) == "devirtualize"
	|| ae.name == "elementmap.devirtualize")
      ae.name = String();
  }
}


void
short_usage()
{
  fprintf(stderr, "Usage: %s [OPTION]... [ROUTERFILE]\n\
Try `%s --help' for more information.\n",
	  program_name, program_name);
}

void
usage()
{
  printf("\
`Click-devirtualize' transforms a router configuration by removing virtual\n\
function calls from its elements' source code. The resulting configuration has\n\
both Click-language files and object files.\n\
\n\
Usage: %s [OPTION]... [ROUTERFILE]\n\
\n\
Options:\n\
  -f, --file FILE              Read router configuration from FILE.\n\
  -e, --expression EXPR        Use EXPR as router configuration.\n\
  -o, --output FILE            Write output to FILE.\n\
  -k, --kernel                 Compile into Linux kernel binary package.\n\
  -u, --user                   Compile into user-level binary package.\n\
  -s, --source                 Write source code only.\n\
  -c, --config                 Write new configuration only.\n\
  -r, --reverse                Reverse devirtualization.\n\
  -n, --no-devirtualize CLASS  Don't devirtualize element class CLASS.\n\
  -i, --instructions FILE      Read devirtualization instructions from FILE.\n\
  -C, --clickpath PATH         Use PATH for CLICKPATH.\n\
      --help                   Print this message and exit.\n\
  -v, --version                Print version number and exit.\n\
\n\
Report bugs to <click@pdos.lcs.mit.edu>.\n", program_name);
}

int
main(int argc, char **argv)
{
  String::static_initialize();
  ErrorHandler::static_initialize(new FileErrorHandler(stderr));
  ErrorHandler *errh = ErrorHandler::default_handler();
  ErrorHandler *p_errh = new PrefixErrorHandler(errh, "click-devirtualize: ");
  CLICK_DEFAULT_PROVIDES;

  // read command line arguments
  Clp_Parser *clp =
    Clp_NewParser(argc, argv, sizeof(options) / sizeof(options[0]), options);
  Clp_SetOptionChar(clp, '+', Clp_ShortNegated);
  program_name = Clp_ProgramName(clp);

  const char *router_file = 0;
  bool file_is_expr = false;
  const char *output_file = 0;
  int source_only = 0;
  int config_only = 0;
  int compile_kernel = 0;
  int compile_user = 0;
  int reverse = 0;
  Vector<const char *> instruction_files;
  HashMap<String, int> specializing;
  
  while (1) {
    int opt = Clp_Next(clp);
    switch (opt) {
      
     case HELP_OPT:
      usage();
      exit(0);
      break;
      
     case VERSION_OPT:
      printf("click-devirtualize (Click) %s\n", CLICK_VERSION);
      printf("Copyright (c) 2000 Massachusetts Institute of Technology\n\
Copyright (c) 2000 Mazu Networks, Inc.\n\
This is free software; see the source for copying conditions.\n\
There is NO warranty, not even for merchantability or fitness for a\n\
particular purpose.\n");
      exit(0);
      break;

     case CLICKPATH_OPT:
      set_clickpath(clp->arg);
      break;
      
     case ROUTER_OPT:
     case EXPRESSION_OPT:
     case Clp_NotOption:
      if (router_file) {
	p_errh->error("router configuration specified twice");
	goto bad_option;
      }
      router_file = clp->arg;
      file_is_expr = (opt == EXPRESSION_OPT);
      break;

     case OUTPUT_OPT:
      if (output_file) {
	p_errh->error("output file specified twice");
	goto bad_option;
      }
      output_file = clp->arg;
      break;
      
     case SOURCE_OPT:
      source_only = !clp->negated;
      break;
      
     case CONFIG_OPT:
      config_only = !clp->negated;
      break;
      
     case KERNEL_OPT:
      compile_kernel = !clp->negated;
      break;
      
     case USERLEVEL_OPT:
      compile_user = !clp->negated;
      break;

     case DEVIRTUALIZE_OPT:
      specializing.insert(clp->arg, !clp->negated);
      break;
      
     case NO_DEVIRTUALIZE_OPT:
      specializing.insert(clp->arg, 0);
      break;

     case INSTRS_OPT:
      instruction_files.push_back(clp->arg);
      break;

     case REVERSE_OPT:
      reverse = !clp->negated;
      break;
      
     bad_option:
     case Clp_BadOption:
      short_usage();
      exit(1);
      break;
      
     case Clp_Done:
      goto done;
      
    }
  }
  
 done:
  if (config_only)
    compile_kernel = compile_user = 0;

  // read router
  RouterT *router = read_router(router_file, file_is_expr, errh);
  if (router)
    router->flatten(errh);
  if (!router || errh->nerrors() > 0)
    exit(1);

  // open output file
  FILE *outf = stdout;
  if (output_file && strcmp(output_file, "-") != 0) {
    outf = fopen(output_file, "w");
    if (!outf)
      errh->fatal("%s: %s", output_file, strerror(errno));
  }

  // handle reversing case
  if (reverse) {
    reverse_transformation(router, errh);
    write_router_file(router, outf, errh);
    exit(0);
  }

  // find and parse `elementmap'
  ElementMap full_elementmap;
  full_elementmap.parse_all_files(router, CLICK_SHAREDIR, errh);

  // initialize signatures
  Signatures sigs(router);

  // follow instructions embedded in router definition
  ElementClassT *devirtualize_info_class = router->get_type("DevirtualizeInfo");
  for (RouterT::type_iterator x = router->first_element(devirtualize_info_class); x; x++) {
    Vector<String> args;
    cp_argvec(x->configuration(), args);
    for (int j = 0; j < args.size(); j++)
      parse_instruction(args[j], sigs, p_errh);
  }

  // follow instructions from command line
  {
    for (int i = 0; i < instruction_files.size(); i++)
      parse_instruction_file(instruction_files[i], sigs, errh);
    for (StringMap::Iterator iter = specializing.first(); iter; iter++)
      sigs.specialize_class(iter.key(), iter.value());
  }

  // choose driver for output
  full_elementmap.check_completeness(router, p_errh);

  String cc_suffix = ".cc";
  String driver_requirement = "";
  if (!full_elementmap.driver_indifferent(router)) {
    bool linuxmodule_ok = full_elementmap.driver_compatible
      (router, Driver::LINUXMODULE);
    bool userlevel_ok = full_elementmap.driver_compatible
      (router, Driver::USERLEVEL);
    if (linuxmodule_ok && userlevel_ok
	&& (compile_kernel > 0) == (compile_user > 0))
      p_errh->fatal("kernel and user-level drivers require different code;\nyou must specify either `-k' or `-u'");
    else if (!linuxmodule_ok && compile_kernel > 0)
      p_errh->fatal("configuration incompatible with kernel driver");
    else if (!userlevel_ok && compile_user > 0)
      p_errh->fatal("configuration incompatible with user-level driver");
    else if (compile_kernel > 0 || (linuxmodule_ok && compile_user <= 0)) {
      cc_suffix = ".k.cc";
      driver_requirement = "linuxmodule ";
      full_elementmap.set_driver(Driver::LINUXMODULE);
    } else {
      cc_suffix = ".u.cc";
      driver_requirement = "userlevel ";
      full_elementmap.set_driver(Driver::USERLEVEL);
    }
  }
  
  // analyze signatures to determine specialization
  sigs.analyze(full_elementmap);
  
  // initialize specializer
  Specializer specializer(router, full_elementmap);
  specializer.specialize(sigs, errh);

  // quit early if nothing was done
  if (specializer.nspecials() == 0) {
    if (source_only)
      errh->message("nothing to devirtualize");
    else
      write_router_file(router, outf, errh);
    exit(0);
  }
  
  // output
  StringAccum out;
  out << "/** click-compile: -w -fno-access-control */\n\
#include <click/config.h>\n\
#include <click/package.hh>\n";
  specializer.output(out);
  
  // find name of package
  String package_name = "devirtualize";
  int uniqueifier = 1;
  while (1) {
    if (router->archive_index(package_name + cc_suffix) < 0)
      break;
    uniqueifier++;
    package_name = "devirtualize" + String(uniqueifier);
  }
  router->add_requirement(package_name);

  specializer.output_package(package_name, out);

  // output source code if required
  if (source_only) {
    fwrite(out.data(), 1, out.length(), outf);
    fclose(outf);
    exit(0);
  }
  
  // create temporary directory
  String tmpdir;
  
  if (compile_user > 0 || compile_kernel > 0) {
    if (!(tmpdir = click_mktmpdir(errh)))
      exit(1);
    
    // find Click binaries
    String click_compile_prog = clickpath_find_file("click-compile", "bin", CLICK_BINDIR, errh);

    // write C++ file
    String cxx_filename = package_name + cc_suffix;
    FILE *f = fopen(tmpdir + cxx_filename, "w");
    if (!f)
      errh->fatal("%s: %s", (tmpdir + cxx_filename).cc(), strerror(errno));
    fwrite(out.data(), 1, out.length(), f);
    fclose(f);

    // write any archived headers
    const Vector<ArchiveElement> &aelist = router->archive();
    for (int i = 0; i < aelist.size(); i++)
      if (aelist[i].name.substring(-3) == ".hh") {
	String filename = tmpdir + aelist[i].name;
	f = fopen(filename, "w");
	if (!f)
	  errh->warning("%s: %s", filename.cc(), strerror(errno));
	else {
	  fwrite(aelist[i].data.data(), 1, aelist[i].data.length(), f);
	  fclose(f);
	}
      }
    
    // compile kernel module
    if (compile_kernel > 0) {
      String compile_command = click_compile_prog + " --directory=" + tmpdir + " --target=kernel --package=" + package_name + ".ko " + cxx_filename;
      int compile_retval = system(compile_command.cc());
      if (compile_retval == 127)
	errh->fatal("could not run `%s'", compile_command.cc());
      else if (compile_retval < 0)
	errh->fatal("could not run `%s': %s", compile_command.cc(), strerror(errno));
      else if (compile_retval != 0)
	errh->fatal("`%s' failed", compile_command.cc());
    }
    
    // compile userlevel
    if (compile_user > 0) {
      String compile_command = click_compile_prog + " --directory=" + tmpdir + " --target=user --package=" + package_name + ".uo " + cxx_filename;
      int compile_retval = system(compile_command.cc());
      if (compile_retval == 127)
	errh->fatal("could not run `%s'", compile_command.cc());
      else if (compile_retval < 0)
	errh->fatal("could not run `%s': %s", compile_command.cc(), strerror(errno));
      else if (compile_retval != 0)
	errh->fatal("`%s' failed", compile_command.cc());
    }
  }

  // retype elements
  specializer.fix_elements();
  
  // read .cc and .?o files, add them to archive
  {
    ArchiveElement ae = init_archive_element(package_name + cc_suffix, 0600);
    ae.data = out.take_string();
    router->add_archive(ae);

    if (compile_kernel > 0) {
      ae.name = package_name + ".ko";
      ae.data = file_string(tmpdir + ae.name, errh);
      router->add_archive(ae);
    }
    
    if (compile_user > 0) {
      ae.name = package_name + ".uo";
      ae.data = file_string(tmpdir + ae.name, errh);
      router->add_archive(ae);
    }
  }
  
  // add elementmap to archive
  {
    if (router->archive_index("elementmap.devirtualize") < 0)
      router->add_archive(init_archive_element("elementmap.devirtualize", 0600));
    ArchiveElement &ae = router->archive("elementmap.devirtualize");
    ElementMap em(ae.data);
    specializer.output_new_elementmap(full_elementmap, em, package_name + cc_suffix, driver_requirement);
    ae.data = em.unparse();
  }

  // add devirtualize_info to archive
  {
    if (router->archive_index("devirtualize_info") < 0)
      router->add_archive(init_archive_element("devirtualize_info", 0600));
    ArchiveElement &ae = router->archive("devirtualize_info");
    StringAccum sa;
    for (int i = 0; i < specializer.nspecials(); i++) {
      const SpecializedClass &c = specializer.special(i);
      if (c.special())
	sa << c.click_name << '\t' << c.old_click_name << '\n';
    }
    ae.data += sa.take_string();
  }
  
  // write configuration
  if (config_only) {
    String s = router->configuration_string();
    fwrite(s.data(), 1, s.length(), outf);
  } else
    write_router_file(router, outf, errh);
  exit(0);
}
