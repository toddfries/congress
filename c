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
our $opt_c; # config file override
our $opt_v; # verbose+++

# defaults
$opt_c = $ENV{HOME}."/.config/govdata/govdata.conf";
$opt_v = 0;

getopts('c:v');

our $verbose = 0;

if ($opt_v>0) {
	$verbose=$opt_v;
}

my $config_file = $opt_c;

my $r = ReadConf->new();

my $config = $r->readconf( $config_file );

our $api_key = $config->{_}->{key};
my $base_url = 'https://api.congress.gov/v3';

our $ua = LWP::UserAgent->new();
$ua->default_header('X-Api-Key' => $api_key, 'format' => 'json');

# Sample request to retrieve a list of bills sorted by date of latest action
my $url = "$base_url/bill";

my $response = make_request($url);

# Parse the JSON response
my $data = decode_json($response->content);

foreach my $bill ( @{ $data->{bills} } ) {

	fmt_bill($bill);

}

# Subroutine to make HTTP requests
sub make_request {
	my ($url) = @_;

	if ($verbose>0) {
		print "make_request ..ooOO( $url )OOoo..\n";
	}

	my $response = $ua->get($url);

	# Check for errors
	unless ($response->is_success) {
		die "Request failed: " . $response->status_line . "\n";
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

	print "Bill Title: $bill->{title}\n";
	print "Bill Info: ";
	print "C".$bill->{congress}." ";
	print $bill->{originChamberCode}." ";
	print $bill->{type}." ";
	print $bill->{number}."\n";
	print "Latest Action: ".$bill->{latestAction}->{actionDate}." ";
	print $bill->{latestAction}->{text}."\n";

	my $bdata = get_info( $bill->{url} );

	print "Sponsor: ";
	fmt_sponsors( $bdata->{bill}->{sponsors} );

	my $amendments = $bdata->{bill}->{amendments};
	my $acount = $amendments->{count};
	if (defined($acount) && $acount > 0) {
		print "Amendments: ${acount}\n";
		$url = $bdata->{bill}->{amendments}->{url};
		my $adata = get_info( $url );
		print Dumper($adata);
	} else {
		print "Amendments: 0\n";
	}

	my $cosponsors = $bdata->{bill}->{cosponsors};
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

	$response = make_request($url);
	$data = decode_json($response->content);

	return $data;
}
