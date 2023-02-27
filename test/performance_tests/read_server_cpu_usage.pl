#!/usr/bin/perl

use strict;
use warnings;

my $CPU_OUT = '/opt/server_cpu_usage.log';
my $line_count = 0;


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

while (<$OUT>) {
  chomp;
  my @line = split(",");
#  print "Line = @line ";

  if ($_ > 1) {
      $cpu_usage += $_;
      $samples++;
  }

  if (($_ > 1) && ($prev_cpu_usage > 1) && ($prev_cpu_usage * 2 < $_)) {
      $cpu_usage -= $prev_cpu_usage;
      $samples--;
  }

  if (($_ > 1) && ($prev_cpu_usage > 1) && ($_ < $prev_cpu_usage * 9 / 10)) {
      $cpu_usage -= $_;
      $samples--;
  }
#  print "samples = $samples\n";
  $line_count++;
  $prev_cpu_usage = $_;
}
close $OUT;

printf "Line count = %d\n", $line_count;
printf "Number of samples = %d\n", $samples;
if ($samples > 0) {
    printf "Server CPU usage: %.2f%%\n", $cpu_usage/$samples;
}
else {
    printf "Server CPU usage: 0%\n";
}


#open my $OUT_TEMP, "<", $CPU_OUT;
#while (<$OUT_TEMP>) {
#    print $CPU_OUT_PERM_LOG $_;
#}
#close $OUT_TEMP;
#close $CPU_OUT_PERM_LOG;

unlink glob $CPU_OUT;
