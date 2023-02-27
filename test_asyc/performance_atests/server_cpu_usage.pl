#!/usr/bin/perl

use strict;
use warnings;

my $CPU_USAGE = "";
my @cmd = "";

#print "arg passed with num of servers:$ARGV[0]\n";
my $servercnt = $ARGV[0];

my $CPU_OUT   = '/opt/server_cpu_usage.log';

if ($ARGV[1] eq "NGINX") {
  $CPU_USAGE = "./cpu_usage_nginx.sh";
  @cmd = ($CPU_USAGE,
             '-t 2',
             '-s',
             '-n',
             $servercnt,
             '>>',
             $CPU_OUT);
  #print "NGINX SCRIPT INVOKE";
} elsif ($ARGV[1] eq "APACHE") {
  $CPU_USAGE = "./cpu_usage.sh";
  @cmd = ($CPU_USAGE,
             '-t 2',
             '-s',
             '>>',
             $CPU_OUT);
} else {
  #print "ERROR:Input NGINX/APACHE to verify CPU usage";
  exit 0;
}

#my $CPU_USAGE = "$ENV{TOOLS_DIR}/cpu_usage.sh";

my $done      = 0;

$SIG{TERM} = sub { $done = 1 };

my $cmd = join(' ', @cmd);

if (fork() == 0) {
  while (!$done) {
    system $cmd;
  }
  print "done!\n";
}

exit 0;

