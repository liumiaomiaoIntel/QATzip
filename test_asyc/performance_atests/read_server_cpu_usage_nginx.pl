#!/usr/bin/perl

use strict;
use warnings;

# Quit unless we have the correct number of command-line args
my $num_args = $#ARGV + 1;
if ($num_args != 1) {
  print "\nUsage: read_server_cpu_usage_nginx.pl number_of_nginx_servers_running\n";
  exit;
}

my $num_servers = $ARGV[0];
my $CPU_OUT = '/opt/server_cpu_usage.log';
my $line_count = 0;
my $NUM_CORES = $num_servers;
my $adjusted_cpu_usage = 0.0;


system "killall -15 server_cpu_usage.pl";

sleep 3; # wait until the process ends

#open my $CPU_OUT_PERM_LOG, ">>", "/opt/server_cpu_usage_permanent.log" or die $!;
open my $TMP_OUT, "<", $CPU_OUT;
my $prev_cpu_usage = <$TMP_OUT>;
close $TMP_OUT;
#printf "First line: %.2f%%\n", $prev_cpu_usage;
$prev_cpu_usage = 0.0 unless ($prev_cpu_usage > 1);
#printf "First line (amended): %.2f%%\n", $prev_cpu_usage;

open my $OUT, "<", $CPU_OUT;

my $cpu_usage = 0.0;
my $samples = 0;
my $ref_val = 0.0;

while (<$OUT>) {
  chomp;
  my @line = split(",");
#  print "Line = @line ";

  if ($_ > 0) {
      $cpu_usage += $_;
      $samples++;
      if ($_ > 50 ) {
          $ref_val = $_;
      }
  }
  #if (($_ > 20) && ($_ < 20) && ($ref_val > 50)) {
      # changes to curtail more samples after req processed
      # 1.4, 1.6, 70, 80, 70, 1.2, 3 (samples should not be increased for count 3)
      # here prev val is 80 and current val is 70, 1.2, 3
  #    if ($_ < 5) {
  #        $cpu_usage -= $_;
  #        $samples--;
  #    }
  #}
  #else {
      #if (($_ > 1) && ($prev_cpu_usage > 1) && ($prev_cpu_usage * 2 < $_)) {
      if (($_ > 0) && ($prev_cpu_usage > 20) && ($prev_cpu_usage * 2 < $_)) {
          $cpu_usage -= $prev_cpu_usage;
          $samples--;
      }

      #if (($_ > 1) && ($prev_cpu_usage > 1) && ($_ < $prev_cpu_usage * 8 / 10)) {
      if (($_ > 0) && ($prev_cpu_usage > 20) && ($_ < $prev_cpu_usage * 8 / 10)) {
          $cpu_usage -= $_;
          $samples--;
      }
  #}
#  print "samples = $samples\n";
  $line_count++;
  $prev_cpu_usage = $_;
}
close $OUT;

printf "Line count = %d\n", $line_count;
printf "Number of samples = %d\n", $samples;
#printf "Number of nginx server workers = %d\n", $num_servers;
if ($samples > 0) {
    $adjusted_cpu_usage = ($NUM_CORES*$cpu_usage/$samples - 0.05*($NUM_CORES - $num_servers))/$num_servers;
    if ($adjusted_cpu_usage > 100.0 ) {
        $adjusted_cpu_usage = 100.0;
    }
    printf "Server CPU usage: %.2f%%\n", $adjusted_cpu_usage;
}

#open my $OUT_TEMP, "<", $CPU_OUT;
#while (<$OUT_TEMP>) {
#    print $CPU_OUT_PERM_LOG $_;
#}
#close $OUT_TEMP;
#close $CPU_OUT_PERM_LOG;

unlink glob $CPU_OUT;
