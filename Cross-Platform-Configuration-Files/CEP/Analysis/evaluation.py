import argparse
import errno
import json
import os
import re

import numpy as np
import numpy_indexed as npi
import seaborn as sns
from matplotlib import pyplot as plt

parser = argparse.ArgumentParser(description='Compare two trace files.')
parser.add_argument('json_config', type=str,
                    help='Location of json config file')
parser.add_argument('trace_file', type=str,
                    help='Real-world trace file')
parser.add_argument('x_variable', type=str,
                    help='The variable that represents the x-axis on the figure')

args = parser.parse_args()

np.set_printoptions(edgeitems=30, linewidth=100000,
                    formatter=dict(float=lambda x: "%.3g" % x))

type_dict = {
    "integer": int,
    "float": float,
    "string": str
}


class TraceEntry(object):
    def __init__(self, line_nr=0, trace_id=0, event_type=0, cpu_id=-1, thread_id=0, timestamp=0, cur_prev_time_diff=0,
                 previous_trace_id=0):
        self.line_nr = line_nr
        self.trace_id = trace_id
        self.event_type = event_type
        self.cpu_id = cpu_id
        self.thread_id = thread_id
        self.timestamp = timestamp
        self.cur_prev_time_diff = cur_prev_time_diff
        self.previous_row = previous_trace_id


class Trace(object):
    def __init__(self, trace, output_fn, trace_ids, reverse_possible_trace_event_transitions, traceAttrs):
        self.rows = []
        self.trace = trace
        self.wb = None
        self.output_fn = output_fn
        self.trace_ids = trace_ids
        self.reverse_possible_trace_event_transitions = reverse_possible_trace_event_transitions
        self.traceAttrs = traceAttrs
        self.numpy_rows = None
        self.max = len(self.rows) + len(self.rows) * 2
        self.cnt = 0
        self.update_bar_trigger = None

    def get_previous_event(self, this_row, previous_rows):
        if this_row.event_type == 2:
            for prev in self.reverse_possible_trace_event_transitions.get(this_row.trace_id):
                for i, row in enumerate(reversed(previous_rows)):
                    if row.trace_id == prev:
                        return row
        else:
            restart = True
            while restart:
                restart = False
                for i, row in enumerate(reversed(previous_rows)):
                    index = -(i + 1)
                    if row.thread_id == this_row.thread_id:
                        previous_row = row
                        del previous_rows[index]
                        return previous_row

        return TraceEntry(timestamp=this_row.timestamp)

    def collect_data(self):
        previous_times = []
        for line_nr, l in enumerate(self.trace):
            split_l = re.split('[\t\n]', l)
            if len(split_l) < len(self.traceAttrs):
                return -1

            try:
                # Depending on the configuration file, the trace event format might be different
                trace_attr = self.traceAttrs['traceId']
                trace_id = str(type_dict[trace_attr['type']](split_l[int(trace_attr['position'])]))
                cpu_attr = self.traceAttrs['cpuId']
                cpu_id = type_dict[cpu_attr['type']](split_l[int(cpu_attr['position'])])
                thread_attr = self.traceAttrs['threadId']
                thread_id = type_dict[thread_attr['type']](split_l[int(thread_attr['position'])])
                timestamp_attr = self.traceAttrs['timestamp']
                timestamp = type_dict[timestamp_attr['type']](split_l[int(timestamp_attr['position'])])
            except ValueError:  # Occurs if any of the casts fail
                return -1

            event_type = self.trace_ids.get(trace_id, {}).get('type', 0)
            # The last two fields are filled in later
            row = TraceEntry(line_nr + 1, trace_id, event_type, thread_id, cpu_id, timestamp, 0, "")
            previous_row = self.get_previous_event(row, previous_times)

            previous_time = previous_row.timestamp
            if trace_id == 1 or line_nr == 0:
                previous_time = row.timestamp
            row.cur_prev_time_diff = timestamp - previous_time
            row.previous_row = previous_row

            numFollowing = self.trace_ids.get(trace_id, {"numFollowing": 1})["numFollowing"]

            self.rows.append(row)

            for _ in range(numFollowing):
                previous_times.append(row)

    def as_plots(self):
        for row in self.rows:
            if row.trace_id == '1' and row.previous_row.trace_id == '7':
                print(row)
        self.numpy_rows = np.array([[te.trace_id, te.thread_id, te.cpu_id, te.timestamp, te.cur_prev_time_diff,
                                     te.previous_row.trace_id, te.event_type] for te in self.rows])
        if len(self.numpy_rows) == 0:
            return
        print(self.numpy_rows)
        print("\n")
        # First group by => to trace ID
        for group in npi.group_by(self.numpy_rows[:, 0]).split(self.numpy_rows):
            # Then group by => from trace ID
            for g2 in npi.group_by(group[:, 5]).split(group[:, :]):
                self.trace_ids.setdefault(str(group[0][0]), {}).setdefault("traced", []).append(
                    {"fromTraceId": g2[0][5], "data": g2})

        trace_file_id = re.split('traces/|[.]trace', self.trace.name)[1]
        try:
            os.mkdir('output/' + trace_file_id)
        except OSError as exc:  # Python >2.5
            if exc.errno == errno.EEXIST and os.path.isdir('output/' + trace_file_id):
                pass
            else:
                raise

        y = []
        xticks = []
        for trace_id, v in self.trace_ids.items():
            for d in v.get("traced", []):
                e = [int(d["data"][i][4]) for i in range(len(d["data"])) if d["data"][i][6] != '2']
                if len(e) > 0:
                    y.append(e)
                    xticks.append(str(d["fromTraceId"]) + "-" + trace_id)

        fig, ax = plt.subplots(figsize=(30, 5))
        x = np.arange(len(xticks))
        plt.xticks(x, xticks)
        ax.plot(x, np.asarray([np.percentile(fifty, 50) for fifty in y]), label='50th percentile')
        ax.plot(x, np.asarray([np.percentile(fourty, 40) for fourty in y]), label='40th percentile')
        ax.plot(x, np.asarray([np.percentile(thirty, 30) for thirty in y]), label='30th percentile')
        ax.plot(x, np.asarray([np.percentile(twenty, 20) for twenty in y]), label='20th percentile')
        ax.plot(x, np.asarray([np.percentile(seventy, 70) for seventy in y]), label='70th percentile')
        ax.plot(x, np.asarray([np.percentile(eighty, 80) for eighty in y]), label='80th percentile')
        ax.plot(x, np.asarray([np.percentile(sixty, 60) for sixty in y]), label='60th percentile')
        ax.plot(x, np.asarray([np.percentile(ten, 10) for ten in y]), label='10th percentile')
        ax.plot(x, np.asarray([np.percentile(one, 1) for one in y]), label='1th percentile')
        ax.plot(x, np.asarray([np.percentile(ninety, 90) for ninety in y]), label='90th percentile')
        plt.title("Processing delay percentiles")
        plt.xlabel("Processing stage")
        plt.ylabel("Processing delay (nanoseconds)")
        fig.savefig('output/' + trace_file_id + '/percentiles.png')
        plt.show()
        plt.cla()

        flattened_y = np.hstack(np.asarray([np.asarray(e) for e in y]).flatten())
        x = []
        for i, e in enumerate(y):
            for _ in e:
                x.append(i)

        plt.title("Processing delay scatter plot")
        plt.xlabel("Processing stage")
        plt.ylabel("Processing delay (nanoseconds)")
        plt.figure(figsize=(30, 5))
        fig = plt.scatter(x, flattened_y).get_figure()

        plt.xticks(range(len(xticks)), xticks)
        fig.savefig('output/' + trace_file_id + '/scatter.png')
        plt.show()

        for toTraceId, v in self.trace_ids.items():
            for g2 in v.get("traced", []):
                try:
                    if g2["fromTraceId"] == "":
                        continue
                    proc_stage = g2["fromTraceId"] + "-" + toTraceId
                    plt.title("Processing delay histogram for processing stage " + proc_stage)
                    plt.xlabel("Processing delay (µs)")
                    plt.ylabel("Number of events")
                    group = np.array([int(r[4]) / 1000 for r in g2["data"]])
                    ninetyninth_perc = np.percentile(group, 99)
                    group = np.array([r for r in group if r < ninetyninth_perc])
                    sns_plot = sns.distplot(group, bins=20, kde=False)
                    fig = sns_plot.get_figure()
                    fig.savefig('output/' + trace_file_id + '/processing-stage-' + proc_stage + '.png')

                    plt.show()
                except np.linalg.LinAlgError:
                    pass


class CompareTraces(object):

    def filter_trace_common_tracepoint_ids(self, t1, t2):
        excluded_tracepoint_ids = []
        t1_map = {}
        filtered_t1 = []
        for line in t1:
            tokens = line.split("\t")
            tracepoint_id = tokens[0]
            # If tracepoint is in the exclusion list, we skip the line
            if tracepoint_id in excluded_tracepoint_ids:
                continue
            # If we have no records of the tracepoint ID yet, we need to make sure that it's in t2 as well.
            # Otherwise, we skip it.
            if t1_map.get(tracepoint_id) is None:
                # We add the tracepoint ID to the exclusion list, and remove it once we know it's in t2
                excluded_tracepoint_ids.append(tracepoint_id)
                # Check if the tracepoint is in t2
                for line2 in t2:
                    tokens2 = line2.split("\t")
                    tracepoint2_id = tokens2[0]
                    if tracepoint_id == tracepoint2_id:
                        # Now we remove the tracepoint ID from the exclusion list because we know it's in t2
                        excluded_tracepoint_ids.pop(-1)
                        break

                # If the tracepoint ID is not in t2, we will now automatically skip it.
                if tracepoint_id in excluded_tracepoint_ids:
                    continue

            filtered_t1.append(line)
            t1_map.setdefault(tracepoint_id, []).append(tokens)

        return filtered_t1, t1_map

    def main(self, json_config, trace_file1, trace_file2, x_variable):
        json_config = json.load(open(json_config))
        scaling_tracepoints = []

        for t in json_config.get("tracepoints"):
            if t.get("x_variable") == x_variable:
                scaling_tracepoints.append(t)

        t1 = list(open(trace_file1))
        t2 = list(open(trace_file2))
        combined_map = {}

        filtered_t1, t1_map = self.filter_trace_common_tracepoint_ids(t1, t2)
        filtered_t2, t2_map = self.filter_trace_common_tracepoint_ids(t2, t1)

        combined_lines = []

        for zipped_line in zip(filtered_t1, filtered_t2):
            #ft1 = re.search(r'\t|\n', zipped_line[0]).groups()
            #ft2 = re.search(r'\t|\n', zipped_line[1]).groups()
            ft1 = re.split("\t|\n", zipped_line[0])
            ft2 = re.split("\t|\n", zipped_line[1])
            combined_lines.append(([int(i) for i in ft1 if i], [int(i) for i in ft2 if i]))

        for k, v in t1_map.items():
            if t2_map.get(k) is not None:
                # We zip 'em
                combined_map[k] = list(zip(v, t2_map[k]))

        #for k, v in combined_map.items():
        #    print("Key:", k)
        #    for tuple in v:
        #        print(k, ":", tuple)

        # The simulation starts Tx packets at 150 seconds, but the real simulation might start after much longer
        offset = combined_lines[0][0][1]-combined_lines[0][1][1]
        #for line in combined_lines:
        #    print(line)
        #    print(line[1][1]+offset-line[0][1])

        scaling_events = {}
        milestone_events = {}
        # Read json config file and populate a dict that contains a mapping between tracepoints and categories
        # We only care about scaling and milestone events, and each event can only be categorized as one of them.
        for t in json_config.get("tracepoints"):
            if t.get("category").get("isScalingEvent"):
                scaling_events[t.get("id")] = True
            if t.get("category").get("isMilestoneEvent"):
                milestone_events[t.get("id")] = True

        x = 0
        # Create figures (Only execution time is relevant for trace)
        # Start with the entire execution time, and then move on to milestone measurements
        # But for all the figures, use the trace event categories so that the system will work even when we make changes

        # Only when a milestone event is processed, will the y value have an effect, which means the y value may go from
        # 1000 to 0, and from 0 to 2000; and on the figure, it'll look like the data goes from 1000 to 2000. That's the
        # intended effect.
        for l in combined_lines:
            e = l[0]
            e2 = l[1]
            #print("t[0]:", t[0], "t2[0]:", t2[0])
            assert e[0] == e2[0], "The tracepoints in the real-world and simulated traces differ"
            if scaling_events.get(e[0]):
                # The event is a scaling event and the x-axis will be affected
                for t in scaling_tracepoints:
                    if t.get("id") == e[0]:
                        #print("This scaling event will affect the x-axis on the figure")
                        scaling_tp = t.get("name")
                        if scaling_tp == "addQuery":
                            x += 1
                        elif scaling_tp == "clearQueries":
                            x = 0
                        elif scaling_tp == "addEvent":
                            x += 1
                        elif scaling_tp == "clearEvents":
                            x = 0
                        else:
                            raise RuntimeError("Unidentified scaling tracepoint")
                        print(e[0], "is a scaling event, x is now", x)
            if milestone_events.get(e[0]):
                #print(e[0], "is a milestone event")
                tracepoint = None
                for t in json_config.get("tracepoints"):
                    if t.get("id") == e[0]:
                        tracepoint = t
                        break

                if tracepoint["name"] == "receiveEvent":
                    print("Event", e[2], "received at", e[1])
                elif tracepoint["name"] == "passedConstraints":
                    print("Event", e[2], "passed constraints at", e[1])
                elif tracepoint["name"] == "createdComplexEvent":
                    print("Event", e[2], "caused the creation of a complex event at", e[1])
                elif tracepoint["name"] == "finishedProcessingEvent":
                    print("Finished processing event", e[2], "at", e[1])


if __name__ == '__main__':
    CompareTraces().main(args.json_config, args.trace_file, args.trace_file+"_simulated", args.x_variable)
