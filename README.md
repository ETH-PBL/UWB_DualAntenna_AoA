<!--
*** Template source: https://github.com/othneildrew/Best-README-Template/blob/master/README.md
-->

<!-- PROJECT SHIELDS -->
<!--
*** I'm using markdown "reference style" links for readability.
*** Reference links are enclosed in brackets [ ] instead of parentheses ( ).
*** See the bottom of this document for the declaration of the reference variables
*** for contributors-url, forks-url, etc. This is an optional, concise syntax you may use.
*** https://www.markdownguide.org/basic-syntax/#reference-style-links
-->
[![Contributors][contributors-shield]][contributors-url]
[![Forks][forks-shield]][forks-url]
[![Stargazers][stars-shield]][stars-url]
[![Issues][issues-shield]][issues-url]
[![License][license-shield]][license-url]


# Angle of Arrival and Centimeter Distance Estimation with a miniaturized UWB Module
> Note: This repository is currently under construction

## Context

## UWB Modules

## Module Assessment and Accuracy

## Dataset

The recorded data is formatted as CSV in the format `TIMESTAMP,DATA_TYPE,DATA`.  

## Error Compensation

# Folder structure
The published data is available in the `data` subfolder.
In this folder, there exists a subfolder for each collected dataset.
The naming scheme of these subfolders is `yyyyMMDD_HHMM_TrainNr_StartStation_NodeNr`.
The date specified is the date of recording, whereas the time given does not necessarily correspond to the start of the recording but rather to the planned departure time of the train.
For example, `20221020_1252_R90120_Formigine_SN1` will contain the dataset recorded on the 20th of October 2022 onboard train R90120 scheduled for departure at 12:52. The train is traveling from Formigine to Modena, and the dataset was recorded using Sensor Node 1.

Each folder contains a `.csv` file containing the raw recorded data as well as a `Readme.md` containing a preliminary analysis of different metrics regarding the given data, such as runtime, GNSS availability, etc. 

## Acknowledges

If you use **Angle of Arrival and Centimeter Distance Estimation with a miniaturized UWB Module** in an academic or industrial context, please cite the following publications:

~~~~
@inproceedings{polonelli2022performance,
  title={Performance Comparison between Decawave DW1000 and DW3000 in low-power double side ranging applications},
  author={Polonelli, Tommaso and Schl{\"a}pfer, Simon and Magno, Michele},
  booktitle={2022 IEEE Sensors Applications Symposium (SAS)},
  pages={1--6},
  year={2022},
  organization={IEEE}
}
~~~~


[contributors-shield]: https://img.shields.io/github/contributors/ETH-PBL/UWB_DualAntenna_AoA.svg?style=flat-square
[contributors-url]: https://github.com/ETH-PBL/UWB_DualAntenna_AoA/graphs/contributors
[forks-shield]: https://img.shields.io/github/forks/ETH-PBL/UWB_DualAntenna_AoA.svg?style=flat-square
[forks-url]: https://github.com/ETH-PBL/UWB_DualAntenna_AoA/network/members
[stars-shield]: https://img.shields.io/github/stars/ETH-PBL/UWB_DualAntenna_AoA.svg?style=flat-square
[stars-url]: https://github.com/ETH-PBL/UWB_DualAntenna_AoA/stargazers
[issues-shield]: https://img.shields.io/github/issues/ETH-PBL/UWB_DualAntenna_AoA.svg?style=flat-square
[issues-url]: https://github.com/ETH-PBL/UWB_DualAntenna_AoA/issues
[license-shield]: https://img.shields.io/github/license/ETH-PBL/UWB_DualAntenna_AoA.svg?style=flat-square
[license-url]: https://github.com/ETH-PBL/UWB_DualAntenna_AoA/blob/master/LICENSE

