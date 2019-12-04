
import csv
import argparse
import yaml
import re

parser = argparse.ArgumentParser(description='Merge two datasets into a yaml format.')
parser.add_argument('yaml_config', type=str,
                    help='Location of yaml config that contains schema')
parser.add_argument('dataset1', type=str,
                    help='Location of the first dataset')
parser.add_argument('dataset2', type=str,
                    help='Location of the second dataset')
parser.add_argument('output_dataset', type=str,
                    help='Location of the resulting dataset')

args = parser.parse_args()


class MergeDatasets(object):
    def __init__(self, dataset1, dataset2, output_dataset, yaml_config):
        self.dataset1 = dataset1
        self.dataset2 = dataset2
        self.output_dataset = output_dataset
        self.yaml_config = yaml_config

    def merge_datasets(self):
        with open(self.yaml_config) as yaml_file:
            yaml_configuration = yaml.load(yaml_file)
        if yaml_configuration is None:
            raise Exception("Failed to open yaml config")
        tuples_dataset1 = []
        with open(self.dataset1) as csv_file:
            dataset_schema = None
            data_stream_row = list(filter(None, re.split('[,\n]', csv_file.readline())))[0]
            for schema in yaml_configuration["stream-definitions"]:
                if schema["stream-id"] == int(data_stream_row):
                    dataset_schema = schema
                    break

            if dataset_schema is None:
                raise Exception("No schema found for dataset 1")
            header_row = list(filter(None, re.split('[,\n]', csv_file.readline())))
            csv_reader = csv.reader(csv_file, delimiter=',')
            for row in csv_reader:
                attributes = row
                t = {
                    "stream-id": int(data_stream_row),
                    "attributes": []
                }
                tuple_format = dataset_schema["tuple-format"]
                for i, attribute in enumerate(attributes):
                    if tuple_format[i]["type"] in ['int', 'long']:
                        attribute = int(attribute)
                    elif tuple_format[i]["type"] in ['double', 'float']:
                        attribute = float(attribute)
                    elif tuple_format[i]["type"] in ['string', 'timestamp']:
                        pass  # By default string
                    else:
                        raise Exception("Unknown attribute type", tuple_format[i]["type"])
                    t["attributes"].append({"name": header_row[i], "value": attribute})
                tuples_dataset1.append(t)

        tuples_dataset2 = []
        with open(self.dataset2) as csv_file:
            dataset_schema = None
            data_stream_row = list(filter(None, re.split('[,\n]', csv_file.readline())))[0]
            for schema in yaml_configuration["stream-definitions"]:
                if schema["stream-id"] == int(data_stream_row):
                    dataset_schema = schema
                    break

            if dataset_schema is None:
                raise Exception("No schema found for dataset 2")
            header_row = list(filter(None, re.split('[,\n]', csv_file.readline())))
            csv_reader = csv.reader(csv_file, delimiter=',')
            for row in csv_reader:
                attributes = row
                t = {
                    "stream-id": int(data_stream_row),
                    "attributes": []
                }
                for i, attribute in enumerate(attributes):
                    if tuple_format[i]["type"] in ['int', 'long']:
                        attribute = int(attribute)
                    elif tuple_format[i]["type"] in ['double', 'float']:
                        attribute = float(attribute)
                    elif tuple_format[i]["type"] in ['string', 'timestamp']:
                        pass  # By default string
                    else:
                        raise Exception("Unknown attribute type", tuple_format[i]["type"])
                    t["attributes"].append({"name": header_row[i], "value": attribute})
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

        new_yaml = {
            "cepevents": new_dataset
        }
        with open(self.output_dataset, 'w') as yaml_output_file:
            yaml.dump(new_yaml, yaml_output_file)


if __name__ == '__main__':
    MergeDatasets(args.dataset1, args.dataset2, args.output_dataset, args.yaml_config).merge_datasets()
