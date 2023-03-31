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
use JSON qw( decode_json );
use LWP::UserAgent;
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

our $ua = LWP::UserAgent->new();
$ua->default_header('X-Api-Key' => $api_key, 'format' => 'json');

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
	foreach my $ut (@tlist) {
		my $url = "$base_url/bill/$maxc/${ut}/${opt_b}";
		my $bdata = get_info( $url );
		if (!defined($bdata)) {
			next;
		}
		fmt_bill( $bdata->{bill} );
		last;
	}
	exit(0);
}


# Default request to retrieve a list of bills sorted by date of latest action
my $url = "$base_url/bill";

my $data = get_info( $url );

foreach my $bill ( @{ $data->{bills} } ) {

	my $bdata = get_info( $bill->{url} );
	fmt_bill($data->{bill});

}

# Subroutine to make HTTP requests
sub make_request {
	my ($url) = @_;

	if ($verbose>0) {
		print "make_request ..ooOO( $url )OOoo..\n";
	}

	my $response = $ua->get($url);
	my $retry = $response->header('retry-after');
	if (defined($retry)) {
		print "Retry header found, pausing ${retry}s\n";
		sleep($retry+1);
		return make_request($url);
	}

	my $thisrlim = $response->header('x-ratelimit-limit');
	my $thisrlim_remain = $response->header('x-ratelimit-remaining');
	if (defined($thisrlim)) {
		$rlim = $thisrlim;
	}
	if (defined($thisrlim_remain)) {
		$rlim_remain = $thisrlim_remain;
	}
	
	# Check for errors
	unless ($response->is_success) {
		if ($verbose>0) {
			print STDERR "Request failed: " . $response->status_line . "\n";
		}
		return $response;
	}

	if ($verbose>0) {
		print Dumper($response);
		print "\n";
	}

	return $response;
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
		my $adata = get_info( $url );
		print Dumper($adata);
	} else {
		print "Amendments: 0\n";
	}

	my $cosponsors = $bill->{cosponsors};
	my $cocount = $cosponsors->{count};
	if (defined($cocount) && $cocount > 0) {
		print "Cosponsors: ${cocount}\n";
		$url = $cosponsors->{url};
		my $codata = get_info( $url );
		fmt_sponsors( $codata->{cosponsors} );
	} else {
		print "Cosponsors: 0\n";
	}

	print "\n";
}

# get bill info
sub get_info {
	my ($url) = @_;

	my $response = make_request($url);
	if ($response->is_success) {
		my $data = decode_json($response->content);
		return $data;
	}
	return undef;
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
