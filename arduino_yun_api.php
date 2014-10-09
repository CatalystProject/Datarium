<?php

	/*
	This file contains the functions to supply the Datarium with movement and colour directives. These actions are informed by data obtained from the 
	Met Office DataPointAPI and cross referencing this data with the energy curve supplied by the Enercon e44 wind turbine. The actions supplied from this
	file are returned as JSON. 
	
	NOTE : You must obtain an API key and location ID from the Met office to use this code.

	Author : Catalyst Project (Peter Newman)
	*/

	//Maximum theoretical output of Tilley wind turbine 
	CONST TILLEY_MAX = 910;
	//energy curve of E44 wind turbine (supplied in Enercon datasheet
	$e44Curve = array(0, 0, 4, 20, 50, 96, 156, 238, 340, 466, 600, 710, 790, 850, 880, 905, 910, 910, 910, 910, 910, 910, 910, 910, 910, 910, 910, 910, 910, 910);	
	//[CHANGE]
	$loc_id = "";
	$api_key = "";
	
	// DataPoint url  
	$url = "http://datapoint.metoffice.gov.uk/public/data/val/wxfcs/all/json/$loc_id?res=3hourly&key=$api_key";	
	
	$arr = get_defined_functions();
	$user_func = $arr['user'];	
		
	//call function name
	$function_name = $_GET['func'];
	if ( in_array( $function_name, $user_func) && function_exists( $function_name )){
		call_user_func($function_name);
	}
	else{
		$params = array( "function" => $function_name );	
		report_error( "Unable to call function.", $params);				
	}
	
	//This function generates random colour values and returns them to the caller
	function test_data(){
		//do random number, 1 - 3, and 1 - 100
		
		$redValue = rand(0, 100);
		$greenValue = 100 - $redValue;
		
		$moveOption = rand(0,2);	
	
		$data = array('red' =>  round($redValue), 'green' =>  round($greenValue), 'move_profile' => $moveOption);
		echo json_encode($data);
	}
	
	//This function looks up the weather and thus renewable energy generated in 2 hours and returns the corresponding colour+ movement as JSON
	function get_24hour_prediction(){
		//make a call to the server, and retrieve latest forecast
		global $url, $e44Curve;
		//current hour
		$time = date('H');
				
		//get json message from DataPoint API and decode JSON
		$response = file_get_contents($url);
		$metJsonMessage = json_decode($response);
		
		//get current hour in minutes
		$intervalTime = intval($time) * 60;
		$day = 1;
	
		//get wind speed (M/S)
		$windSpeed = get_wind_speed($day, $intervalTime);
				
		//get percentage of maximum output
		$output = $e44Curve[$windSpeed];
		$percent = $output / TILLEY_MAX * 100;
		
		//adjust percent to skew towards more green (Green LEDs less brighter than red ones)
		$percent *= 3;
		if ( $percent > 100 )
			$percent = 100;
				
		//get red and green values
		$redValue = 100 - $percent;
		$greenValue = $percent;
	
		//format message and sent back to client
		$data = array('red' => round($redValue), 'green' => round($greenValue), 'move_profile' => 1);
		
		// create curl resource 
        $ch = curl_init(); 
        // set url 
        curl_setopt($ch, CURLOPT_URL, "http://tiree-onsupply.co.uk/dd/aya_stub.php?func=log_datarium"); 
		//return the transfer as a string 
        curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1); 
		curl_exec($ch); 
		
		//output JSON message to caller
		echo json_encode($data);
	}
	
	//This function gets the current expected wind turbine yield and returns the colours to show via a JSON message
	function get_current_hour(){
		//make a call to the server, and retrieve latest forecast
		global $e44Curve;
		//current hour
		$time = date('H');
		
		//get current hour in minutes -	//use this time to get the closest time prediction.
		$intervalTime = intval($time) * 60;
		$day = 0;		
			
		//get wind speed (in M/S)
		$windSpeed = get_wind_speed($day, $intervalTime);

		$output = $e44Curve[$windSpeed];
		$percent = $output / TILLEY_MAX * 100;
		
		//adjust percent to skew towards more green
		$percent *= 3;
		if ( $percent > 100 )
			$percent = 100;
		
		//get red and green values
		$redValue = 100 - $percent;
		$greenValue = $percent;
	
		//format message and sent back to client
		$data = array('red' => round($redValue), 'green' => round($greenValue), 'move_profile' => 0);
		echo json_encode($data);
	}	
	
	//This function returns the wind speed given a day integer and and time
	function get_wind_speed($day, $intervalTime){
		global $url, $e44Curve;
	
		//get message from DataPoint API - could be replaced..
		$response = file_get_contents($url);
		$metJsonMessage = json_decode($response);
		
		//get current day (first result in list)
		$days = $metJsonMessage->SiteRep->DV->Location->Period;
		$periods = $days[$day]->Rep;		
		
		$period = NULL;
		$smallestDelta = -1;
		//using these forecast points, find the closest time period and pass back to client
		for ($i = 0; $i < count($periods); $i++ ){
			$timePeriod = intval($periods[$i]->{"$"});
			
			$delta = abs($intervalTime - $timePeriod);
			
			if ( $smallestDelta == -1 || $delta < $smallestDelta ){
				$smallestDelta = $delta;
				$period  = $periods[$i];			
			}
		}
		
		//make sure we got a period
		if ( $period == NULL ){
			$params = array( "function" => "getCurrentHour" );	
			report_error( "Unable to get data point for current time.", $params);	
		}
		
		//get predicted power based on wind speed, and send this back to the client
		$windSpeed = round(intval($period->S) * 0.44704);

		if ( $windSpeed > count($e44Curve) )
			$windSpeed = count($e44Curve) - 1;
		
		return $windSpeed;	
	}
	
	//function used to send error message back as JSON to client.
	function report_error( $msg, $array_args){	
		$array_args["msg"] = $msg;
		echo json_encode($array_args);
	}
?>