
import csv
import argparse
import json
import re

parser = argparse.ArgumentParser(description='Merge two datasets into a json format.')
parser.add_argument('dataset1', type=str,
                    help='Location of the first dataset')
parser.add_argument('dataset2', type=str,
                    help='Location of the second dataset')
parser.add_argument('outputDataset', type=str,
                    help='Location of the resulting dataset')

args = parser.parse_args()


class MergeDatasets(object):
    def __init__(self, dataset1, dataset2, outputDataset):
        self.dataset1 = dataset1
        self.dataset2 = dataset2
        self.outputDataset = outputDataset

    def merge_datasets(self):
        tuples_dataset1 = []
        with open(self.dataset1) as csv_file:
            header_row = list(filter(None, re.split('[,\n]', csv_file.readline())))
            csv_reader = csv.reader(csv_file, delimiter=',')
            for row in csv_reader:
                attributes = row
                t = {
                    "stream-id": attributes[0],
                    "attributes": []
                }
                for i, attribute in enumerate(attributes):
                    t["attributes"] = {"name": header_row[i], "value": attribute}
                tuples_dataset1.append(t)

        tuples_dataset2 = []
        with open(self.dataset2) as csv_file:
            header_row = list(filter(None, re.split('[,\n]', csv_file.readline())))
            csv_reader = csv.reader(csv_file, delimiter=',')
            for row in csv_reader:
                attributes = row
                t = {
                    "stream-id": attributes[0],
                    "attributes": []
                }
                for i, attribute in enumerate(attributes):
                    t["attributes"] = {"name": header_row[i], "value": attribute}
                tuples_dataset2.append(t)

        if len(tuples_dataset2) > len(tuples_dataset1):
            # tuples_dataset1 must be the largest one
            tuples_dataset1, tuples_dataset2 = tuples_dataset2, tuples_dataset1

        new_dataset = []
        for t1 in tuples_dataset1:
            new_dataset.append(t1)
            if len(tuples_dataset2) > 0:
                t2 = tuples_dataset2[0]
                tuples_dataset2 = tuples_dataset2[1:]
                new_dataset.append(t2)

        new_json = {
            "cepevents": new_dataset
        }
        with open(self.outputDataset, 'w') as json_output_file:
            json.dump(new_json, json_output_file)


if __name__ == '__main__':
    MergeDatasets(args.dataset1, args.dataset2, args.outputDataset).merge_datasets()
