#!/usr/bin/perl

# Copyright (c) 2023 Todd T. Fries <todd@fries.net>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

use strict;
use warnings;

use Data::Dumper;
use Getopt::Std;
use Gov::Data;
use ReadConf;

# definitions
our $opt_b; # look up a single bill
our $opt_c; # config file override
our $opt_t; # bill type
our $opt_v; # verbose+++

# defaults
$opt_c = $ENV{HOME}."/.config/govdata/govdata.conf";
$opt_v = 0;

getopts('b:c:t:v');

our $verbose = $opt_v;
our $rlim = 0;
our $rlim_remain = 0;

my $config_file = $opt_c;

my $r = ReadConf->new();

my $config = $r->readconf( $config_file );

our $api_key = $config->{_}->{key};
my $base_url = 'https://api.congress.gov/v3';
our $maxc = $config->{_}->{max_congress_seen};
if (!defined($maxc)) {
	$maxc = 0;
}

my $info = {
	'ApiKey' => $api_key,
	'format' => 'json',
	'UserAgent' => 'perl c(ongress)/0.1',
	'verbose' => $verbose,
};

our $gd = Gov::Data->new($info);

# some defaults to aid in parsing
our @billtypes = ("H", "HR", "S", "HJRES", "SJRES", "HCONRES", "SCONRES", "HRES", "SRES");

if (defined($opt_t)) {
	my $ut = uc($opt_t);
	my @bmatch = grep {$ut eq $_} @billtypes;
	if (!@bmatch) {
		die "not a valid bill type: $ut";
	}
}

if (defined($opt_b)) {
	my @tlist = @billtypes;
	if (defined($opt_t)) {
		@tlist = ( uc($opt_t) );
	}
	my $found = 0;
	my $myc = $maxc;
	while ($myc > 0) {
	    foreach my $ut (@tlist) {
		my $url = "$base_url/bill/$myc/${ut}/${opt_b}";
		my $bdata = $gd->get_info( $url );
		if (!defined($bdata)) {
			next;
		}
		$found++;
		fmt_bill( $bdata->{bill} );
		last;
	    }
	    if ($found > 0) {
		last;
	    }
	    print " no bills matching ${opt_b} in congress#${myc}\n";
	    $myc--;
	    show_limits();
	    sleep(1);
	}
	exit(0);
}


# Default request to retrieve a list of bills sorted by date of latest action
my $url = "$base_url/bill";

my $data = $gd->get_info( $url );

foreach my $bill ( @{ $data->{bills} } ) {

	my $bdata = $gd->get_info( $bill->{url} );
	fmt_bill($data->{bill});

}

sub fmt_actions {
	my ($actions) = @_;
	foreach my $a (@{ $actions }) {
		print " ";
		print $a->{actionDate}." ";
		if (defined($a->{actionCode})) {
			print $a->{actionCode}." ";
		}
		print $a->{type}." ";
		print $a->{text}."\n";
		foreach my $vote (@{$a->{recordedVotes}}) {
			print " ";
			print $vote->{date}." ";
			my $data = $gd->get_xml($vote->{url});
			if (defined($data->{count}->{yeas})) {
				print " Yeas/Nays = ".$data->{count}->{yeas}."/".$data->{count}->{nays}."\n";
			} elsif (defined($data->{'vote-metadata'}->{'vote-question'})) {
				my $vmd = $data->{'vote-metadata'};
				print " Question: ";
				print $vmd->{'vote-question'}." ";
				print " Yeas/Nays/Abstain = ";
				print $vmd->{'vote-totals'}->{'totals-by-vote'}->{'yea-total'}."/";
				print $vmd->{'vote-totals'}->{'totals-by-vote'}->{'nay-total'}."/";
				print $vmd->{'vote-totals'}->{'totals-by-vote'}->{'not-voting-total'}."\n";
			} else {
				print Dumper($data)."\n";
			}
		}
	}
}

# format sponsors
sub fmt_sponsors {
	my ($slist) = @_;

	my $i = 0;
	foreach my $s ( sort { $a->{lastName} cmp $b->{lastName} } @{ $slist } ) {
		print " ";
		print $s->{state}." ";
		print $s->{party}." ";
		print $s->{lastName}.", ";
		print $s->{firstName}."\n";
		$i++;
	}
	if ($i < 1) {
		print "\n";
	}
}

# format bill
sub fmt_bill {
	my ($bill) = @_;

	# looks like originChamberCode not set for bill/<c>/<type>/<no>
	# but is set for bill api calls
	if (!defined($bill->{originChamberCode})) {
		if (!defined($bill->{originChamber})) {
			$bill->{originChamberCode} = "U"; # unknown
		} else {
			if ($bill->{originChamber} eq "Senate") {
				$bill->{originChamberCode} = "S";
			} elsif ($bill->{originChamber} eq "House") {
				$bill->{originChamberCode} = "H";
			} else {
				$bill->{originChamberCode} = "U";
			}
		}
	}

	print "Bill Title: $bill->{title}\n";
	print "Bill Info: ";
	print "C".$bill->{congress}." ";
	update_maxc( $bill->{congress} );
	print $bill->{originChamberCode}." ";
	print $bill->{type}." ";
	print $bill->{number}."\n";
	print "Latest Action: ".$bill->{latestAction}->{actionDate}." ";
	print $bill->{latestAction}->{text}."\n";


	print "Sponsor: ";
	fmt_sponsors( $bill->{sponsors} );

	my $amendments = $bill->{amendments};
	my $acount = $amendments->{count};
	if (defined($acount) && $acount > 0) {
		print "Amendments: ${acount}\n";
		$url = $bill->{amendments}->{url};
		my $adata = $gd->get_info( $url );
	} else {
		print "Amendments: 0\n";
	}

	my $cosponsors = $bill->{cosponsors};
	my $cocount = $cosponsors->{count};
	if (defined($cocount) && $cocount > 0) {
		print "Cosponsors: ${cocount}\n";
		$url = $cosponsors->{url};
		my $codata = $gd->get_info( $url );
		fmt_sponsors( $codata->{cosponsors} );
	} else {
		print "Cosponsors: 0\n";
	}

	my $actions = $bill->{actions};
	my $actcount = $actions->{count};
	if (defined($actcount) && $actcount > 0) {
		print "Actions: ${actcount}\n";
		my $adata = $gd->get_info( $actions->{url} );
		fmt_actions( $adata->{actions} );
	} else {
		print "Actions: 0\n";
	}
	print "\n";
}

# error check?
sub update_maxc {
	my ($c) = @_;

	if ($c <= $maxc) {
		return;
	}
	my $tmpfile = $opt_c.".tmp";

	$maxc = $c;
	open(C, "<", $opt_c);
	open(N, ">", $tmpfile);
	my $mcfound = 0;
	while (<C>) {
		if (/^maxc/) {
			print N "max_congress_seen = ${maxc}\n";
			next;
		}
		print N $_;
	}
	close(C);
	if ($mcfound < 1) {
		print N "max_congress_seen = ${maxc}\n";
	}
	close(N);
	rename($opt_c.".tmp",$opt_c);
}

sub show_limits {
	printf "   %s/%s left\n", $gd->{rlim_remain}, $gd->{rlim};
}
