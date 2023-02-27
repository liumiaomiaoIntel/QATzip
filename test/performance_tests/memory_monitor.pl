#!/usr/bin/perl -w

use strict;
use POSIX qw(strftime);

my $lower_free_mem = 1<<40;
my $done = 0;

$SIG{TERM} = sub { $done = 1 };

sub print_memory_usage {
  my $out = shift;
  my $free = `free`;
	$free =~ m/Mem:\s+(.*)/g;
  my @mem_values = split /\s+/, $1;
  my $actual_free_mem = $mem_values[2];
  if ($actual_free_mem < $lower_free_mem) {
    $lower_free_mem = $actual_free_mem; 
  }

  $free =~ m/cache:(.*)/g;
  my $cache = $1;
  $cache =~ s/\s+/,/g;
  my $time = strftime "%Y/%m/%d %H:%M:%S", localtime;
  printf $out "%s,%s%s,%s\n", $time, 
    join(',', @mem_values),$cache, $lower_free_mem;
}

sub main {
  my $filename = strftime "%Y%m%d_%H%M%S", localtime;
  
open (OUTFILE,">./mem_file_name.txt");
print OUTFILE "/opt/memory-monitor-$filename.csv";
close(OUTFILE);
  
  open my $OUT, ">/opt/memory-monitor-$filename.csv";
  
  my $start_time = time;

  print $OUT "time,total(KB),used(KB),free(KB),shared(KB),",
    "buffers(KB),cached(KB),used buffers/cache(KB),",
    "free buffers/cache(KB),lower free memory(KB)\n";

  while (!$done) {
    print_memory_usage($OUT);
    sleep 10;
  }

  close $OUT;
}

if (fork() == 0) {
  main();
}
