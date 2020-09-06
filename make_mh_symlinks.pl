#!/usr/bin/perl

opendir(DIR, ".") || die;
@files = readdir(DIR);
closedir(DIR);

@files = grep {/^\.\d+$/} @files;
@files = map { substr $_, 1 } @files;
@files = sort { $a <=> $b } @files;

$i = 1;
foreach $file (@files) {
    print "ln -s .$file $i\n";
    $i++;
}

